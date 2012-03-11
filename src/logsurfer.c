/*
 * logsurfer
 *
 * Authors: Wolfgang Ley, Uwe Ellermann, Kerry Thompson
 */


/* config header if available */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


/* system includes */

#include <stdio.h>

#if HAVE_STDLIB_H || STDC_HEADERS
#include <stdlib.h>
#endif

/* ugly, but portable */ 
#if HAVE_STRING_H || STDC_HEADERS
#include <string.h>
#else
#if HAVE_STRINGS_H
#include <strings.h>
#else
#if HAVE_STRCHR
char *strchr();
#else
#if HAVE_INDEX
#define strchr index
char *strchr();
#endif
#endif
#endif
#endif

#include <ctype.h>

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <paths.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#if HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_MALLOC_H
#include <malloc.h>
#endif

#include <sys/errno.h>
#include <fcntl.h>

/* local includes */

#include "logsurfer.h"
#include "globals.h"
#include "regex.h"
#include "str_util.h"
#include "context.h"
#include "rule.h"
#include "exec.h"
#include "exit.h"
#include "readline.h"
#include "readcf.h"


/*
 * the global variables
 */

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

struct rule	*all_rules;	/* all active rules		*/
struct rule	*all_rules_end;
struct context	*all_contexts;	/* all active contexts		*/
struct context	*all_contexts_end;
long		next_rule_timeout;	/* next rule timeout	*/
long		next_context_timeout;	/* next context timeout	*/

char		logfile_name[MAXPATHLEN];	/* name of the logfile	*/
FILE		*logfile;		/* input logfile	*/
long		logline_num;	/* linenumer of the logline	*/
char		*logline;	/* the actual logline		*/
struct context_line *logline_context;/* the context_line ptr	*/
time_t		current_time;	/* current time information	*/

char		dumpfile_name[MAXPATHLEN];	/* name of the dumpfile */
char		pidfile_name[MAXPATHLEN];	/* name of the pid file */

int		exit_silent=0;		/* exit silently?	*/

int             timeout_contexts_at_exit=0;     /* timeout contexts when exit? */

struct stat ostb;    /* Logfile attributes, used for -F option */
struct stat nstb;    /* Logfile attributes, used for -F option */

/*
 * definitions for the regular expression matching routines
 *
 * we have an array with submatches and the number of valid submatches
 */
struct re_registers regex_submatches;
struct re_registers regex_notmatches;
int 		regex_submatches_num;

/*
 * new argv and argc for the externals programs we are starting
 */
char		*new_argv[256];	/* build a new argv[] list 	*/
int		new_argc=0;

#ifdef SENDMAIL_FLUSH
/*
 * should we flush the mail-queue?
 */
long		flush_time;

#endif	/* SENDMAIL_FLUSH */



/*
 * process a line from the logfile
 */
void
process_logline()
{
	struct context		*this_context;
	struct rule		*this_rule, *this_rule_next;
	int			check_stop=0;

	if ( logline[0] == '\0' )
		return;
	if ( logline[strlen(logline)-1] == '\n' )
		logline[strlen(logline)-1]='\0';

#ifdef DEBUG
(void) printf("processing logline: %s\n", logline);
#endif
	if ( (logline_context=(struct context_line *)malloc(sizeof(struct context_line)))
		== NULL ) {
		(void) fprintf(stderr, "out of memory processing logling: %s\n",
			logline);
		return;
	}
	if ( (logline_context->content=(char *)malloc(strlen(logline)+1)) == NULL ) {
		(void) fprintf(stderr, "out of memory processing logling: %s\n",
			logline);
		(void) free(logline_context);
		return;
	}
	logline_context->linenumber=logline_num;
	logline_context->timestamp=(long) current_time;
	(void) strcpy(logline_context->content, logline);
	logline_context->link_counter=1;

	/*
	 * first try to store the logline in all matching contexts
	 */
	for ( this_context=all_contexts; this_context != NULL;
		this_context=this_context->next)
		if ( re_search(this_context->match_regex, logline, strlen(logline),
			0, strlen(logline), &regex_submatches) >= 0 ) {
			regex_submatches_num=this_context->match_regex->re_nsub;
			if ( this_context->match_not_regex != NULL ) {
				if ( re_search(this_context->match_not_regex, logline,
					strlen(logline), 0, strlen(logline),
					&regex_notmatches) == -1 ) {
						add_to_context(this_context, logline_context);
				}
			}
			else
				add_to_context(this_context, logline_context);
		}

	/*
	 * now check if there is a matching rule for this logline
	 */
	for ( this_rule=all_rules; this_rule != NULL ; ) {
		/* check if the logline matches the first rgex */
		if ( re_search(this_rule->match_regex, logline, strlen(logline),
			0, strlen(logline), &regex_submatches) >= 0 ) {
			regex_submatches_num=this_rule->match_regex->re_nsub;
			this_rule_next=this_rule->next;
			/* if we do have a not-match regex test it */
			if ( this_rule->match_not_regex != NULL ) {
				if ( re_search(this_rule->match_not_regex, logline,
					strlen(logline), 0, strlen(logline),
					&regex_notmatches) == -1 ) {
						check_stop=1;
						if ( this_rule->do_continue == 0 )
							this_rule_next=NULL;
						process_rule(this_rule);
				}
			}
			else {
				check_stop=1;
				if ( this_rule->do_continue == 0 )
					this_rule_next=NULL;
				process_rule(this_rule);
			}
			if ( check_stop == 1 ) {
				/* check for the stop_regex */
				if ( this_rule->stop_regex != NULL ) {

			if ( re_search(this_rule->stop_regex, logline, strlen(logline),
				0, strlen(logline), &regex_submatches) >= 0 ) {
				/* if we do have a not-match stop-regex test it */
				if ( this_rule->stop_not_regex != NULL ) {
					if ( re_search(this_rule->stop_not_regex,
						logline, strlen(logline), 0,
						strlen(logline), &regex_notmatches) == -1 )
						unlink_rule(this_rule);
				}
				else
					unlink_rule(this_rule);
			}

				}
				check_stop=0;
			}
			this_rule=this_rule_next;
		}
		else
			this_rule=this_rule->next;
	}

	logline_context->link_counter--;
	/*
	 * free the new line if not used in any context
	 */
	destroy_context_line(logline_context);

	return;
}


/*
 * reopen the input logfile
 */
void
logfile_reopen(sig)
	int	sig;
/* ARGSUSED */
{
	if ( logfile_name[0] == '\0' )
		return;
	if ( logfile != NULL )
		(void) fclose(logfile);
	if ( (logfile=fopen(logfile_name, "r")) == NULL ) {
		(void) fprintf(stderr, "error reopening logfile %s\n",
			logfile_name);
		exit(6);
	}
	return;
}

/*
 * Fork and run in the background
 */
int
runasdaemon(lfd)
    int lfd;
{
    int fd = -1;
    int maxfd = -1;

    errno = 0;

    switch (fork()) {
        case -1:
            (void) fprintf(stderr, "could not daemonize: fork(): %s\n", strerror(errno));
            exit(11);
        case 0:
            break;
        default:
            exit(0);
    }

    if ( setsid() < 0 ) {
        (void) fprintf(stderr, "could not daemonize: setsid(): %s\n", strerror(errno));
        exit(6);
    }

    switch (fork()) {
        case -1:
            (void) fprintf(stderr, "could not daemonize: fork(): %s\n", strerror(errno));
            exit(11);
        case 0:
            break;
        default:
            exit(0);
    }

    if ( (fd = open(_PATH_DEVNULL, O_RDWR, 0)) < 0) {
        (void) fprintf(stderr, "could not daemonize: %s\n", strerror(errno));
        exit(11);
    }

    if ( dup2(fd, STDIN_FILENO) == -1 ||
         dup2(fd, STDOUT_FILENO) == -1 ||
         dup2(fd, STDERR_FILENO) == -1 ) {
        (void) fprintf(stderr, "could not daemonize: %s\n", strerror(errno));
        exit(11);
    }

    /* (void) chdir("/"); */

#define MAXFD   1024
    if ( (maxfd = getdtablesize()) < 0)
        maxfd = MAXFD;

    for ( fd=STDERR_FILENO+1; fd<maxfd; fd++ ) {
        if (fd != lfd)
            (void) close(fd);
    }
}

/*
 * print usage information and exit
 */
void
usage(progname)
	char	*progname;
{
	(void) fprintf(stderr,
		"usage: %s [-l startline | -r startregex] [-c configfile] [-d dumpfile] [-p pidfile] [-D] [-f] [-F] [-t] [-e] [logfile]\n",
		progname);
	(void) fprintf(stderr, "This is Logsurfer version %s\n", VERSION );
	exit(1);
}


/*
 * main program
 */
int
main(argc, argv)
	int	argc;
	char	**argv;
{
	char		*progname;	/* name of this program		*/
	int		opt;		/* used for options processing	*/
	extern char	*optarg;	/* option argument		*/
	extern int	optind;		/* option index			*/

	char		cf_filename[MAXPATHLEN]; /* configuration filename */
	long		start_line=0;		/* startline within logfile */
	int             start_at_end=0;         /* start at end of file     */
	struct re_pattern_buffer *start_regex=NULL; /* regex for startline  */
	int		do_follow=0;		/* follow the file?	*/
	int		daemonize=0;		/* run in the background */
    int     write_pidfile=0;    /* flag to write PID to file */
	FILE		*pidfile;		/* write pid info to file */

	char		*logline_buffer;	/* buffer for reading	*/
	int		logline_buffer_size;	/* size of buffer	*/
	int		logline_buffer_pos;	/* position within buffer */

	struct sigaction	signal_info;	/* used by sigaction()	*/


	if ( (progname=strrchr(argv[0], '/')) == NULL )
		progname=argv[0];
	else
		progname++;

	/* setup some defaults and initialize the global pointers */
	(void) strcpy(logfile_name, "");
	(void) strcpy(cf_filename, CONFFILE);
	(void) strcpy(dumpfile_name, DUMPFILE);
	all_rules=NULL;
	all_rules_end=NULL;
	all_contexts=NULL;
	all_contexts_end=NULL;
	next_rule_timeout=LONG_MAX;
	next_context_timeout=LONG_MAX;
	logline_num=0;
	logline=NULL;
    (void) memset(&ostb, 0, sizeof(struct stat));
    (void) memset(&nstb, 0, sizeof(struct stat));

#ifdef SENDMAIL_FLUSH
	flush_time=LONG_MAX;
#endif

	(void) time(&current_time);

#ifdef WARN_ROOT
	if ( getuid() == 0 )
		(void) fprintf(stderr, "warning: %s started as root\n", progname);
#endif

	/*
	 * we want regular expression macthing like egrep
	 */
	re_syntax_options=RE_SYNTAX_POSIX_EGREP;

	/*
	 * get memory to store the submatches for a regex-match
	 */
	regex_submatches.num_regs=RE_NREGS;
	if ( (regex_submatches.start=(regoff_t *)
		malloc(RE_NREGS*sizeof(regoff_t))) == NULL ) {
		(void) fprintf(stderr, "unable to allocate memory for submatches\n");
		exit(99);
	}
	if ( (regex_submatches.end=(regoff_t *)
		malloc(RE_NREGS*sizeof(regoff_t))) == NULL ) {
		(void) fprintf(stderr, "unable to allocate memory for submatches\n");
		exit(99);
	}
	regex_notmatches.num_regs=RE_NREGS;
	if ( (regex_notmatches.start=(regoff_t *)
		malloc(RE_NREGS*sizeof(regoff_t))) == NULL ) {
		(void) fprintf(stderr, "unable to allocate memory for notmatches\n");
		exit(99);
	}
	if ( (regex_notmatches.end=(regoff_t *)
		malloc(RE_NREGS*sizeof(regoff_t))) == NULL ) {
		(void) fprintf(stderr, "unable to allocate memory for notmatches\n");
		exit(99);
	}

	/*
	 * get some memory for the logline_buffer
	 */
	logline_buffer_size=4096;
	logline_buffer_pos=0;
	if ( (logline_buffer=(char *)malloc(logline_buffer_size)) == NULL ) {
		(void) fprintf(stderr, "unable to allocate memory for logline_buffer\n");
		exit(99);
	}

	while ( (opt = getopt(argc, argv, "efFc:d:Dl:p:r:st")) != EOF )
		switch(opt) {
                case 'e':
		        /* start processing at the end of the log file */
		        start_at_end=1;
		        break;
		case 'f':
			/* set follow mode on */
			do_follow=1;
			break;
        case 'F':
            /* set follow obsessively mode on (follow rotated files) */
            do_follow=2;
            break;
		case 'c':
			/* specify another config file */
			(void) strcpy(cf_filename, optarg);
			break;
		case 'd':
			/* specify another dump file */
			(void) strcpy(dumpfile_name, optarg);
			break;
		case 'D':
			/* daemonize: close fd's and run in the background */
            daemonize=1;
			break;
		case 'l':
			/* start processing beginning with given line number */
			if ( start_regex != NULL ) {
				(void) fprintf(stderr, "-l and -r can't be used together\n");
				usage(progname);
			}
            if ( start_at_end != 0 ) {
                (void) fprintf(stderr, "-e and -l can't be used together\n");
                usage(progname);
            }
			start_line=atol(optarg);
			break;
		case 'p':
            write_pidfile=1;
            (void) strcpy(pidfile_name, optarg);
			break;
		case 'r':
			/* start processing at first line that matches regex */
			if ( start_line != 0 ) {
				(void) fprintf(stderr, "-l and -r can't be used together\n");
				usage(progname);
			}
            if ( start_at_end != 0 ) {
                (void) fprintf(stderr, "-e and -r can't be used together\n");
                usage(progname);
            }
			if ( (start_regex=(struct re_pattern_buffer *)
				malloc(sizeof(struct re_pattern_buffer))) == NULL ) {
				(void) fprintf(stderr, "out of memory for start regex %s",
					optarg);
				exit(99);
			}

			start_regex->fastmap=(char *)malloc(256);
			start_regex->translate=(char *) 0;
			start_regex->buffer=NULL;
			start_regex->allocated=0;
			if ( re_compile_pattern(optarg, strlen(optarg), start_regex) != 0 ) {
				(void) fprintf(stderr, "error in start-regex %s\n", optarg);
				exit(5);
			}
			start_regex->regs_allocated=REGS_FIXED;
			break;
		case 's':
			exit_silent=1;
			break;
		case 't':
		        timeout_contexts_at_exit=1;
			break;
		default:
			usage(progname);
			break;
		}

	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage(progname);

	if ( read_config(cf_filename) != 0 ) {
		(void) fprintf(stderr, "error reading configfile %s\n", cf_filename);
		exit(2);
	}

	if ( argc == 0 || !strcmp(argv[0], "-") ) {
        if (daemonize) {
			(void) fprintf(stderr, "cannot use stdin when running as a daemon\n");
			exit(6);
        }
		logfile=stdin;
    }
	else {
		if ( (logfile=fopen(strcpy(logfile_name, argv[0]), "r")) == NULL ) {
			(void) fprintf(stderr, "error opening logfile %s\n",
				logfile_name);
			exit(6);
		}
		if (start_at_end) {
		  fseek(logfile,0,SEEK_END);
		}
        if ( do_follow == 2 ) {
            if ( fstat(fileno(logfile), &ostb) == -1 ) {
                (void) fprintf(stderr, "cannot stat logfile\n");
                exit(6);
            }
        }
	}

    if ( daemonize )
        runasdaemon(fileno(logfile));

    if( write_pidfile ){
        /* write pid to a file */
        if ( (pidfile=fopen(pidfile_name, "w")) == NULL ) {
            (void) fprintf(stderr, "unable to open pidfile %s\n", pidfile_name);
            exit(9);
        }
        (void) fprintf(pidfile, "%ld\n", (long) getpid());
        (void) fclose(pidfile);
    }

	if ( start_line != 0 ) {
		logline_num=start_line-1;
		while ( --start_line > 0 ) {
			if ( (logline=readline(logfile, &logline_buffer,
				&logline_buffer_size, &logline_buffer_pos))
				== NULL ) {
				(void) fprintf(stderr,
					"not enough input lines to skip\n");
				exit(7);
			}
			(void) free(logline);
		}
	}
	else while ( start_regex != NULL ) {
		if ( (logline=readline(logfile, &logline_buffer, &logline_buffer_size,
			&logline_buffer_pos)) == NULL ) {
			(void) fprintf(stderr,
				"unable to find inputline for start_regex\n");
			exit(7);
		}
		logline_num++;
		if ( re_search(start_regex, logline, strlen(logline), 0,
			strlen(logline), &regex_submatches) >= 0 ) {
			regfree(start_regex);
			(void) free(start_regex);
			start_regex=NULL;
			process_logline();
		}
		(void) free(logline);
	}

	signal_info.sa_handler=dump_data;
	(void) sigemptyset(&(signal_info.sa_mask));

#ifdef SA_RESTART
	signal_info.sa_flags=SA_RESTART;
#else
	signal_info.sa_flags=0;
#endif

	if ( sigaction(SIGUSR1, &signal_info, NULL) ) {
		(void) fprintf(stderr, "can't set signal to handle USR1 - abort\n");
		exit(8);
	}
	signal_info.sa_handler=logfile_reopen;
	if ( sigaction(SIGHUP, &signal_info, NULL) ) {
		(void) fprintf(stderr, "can't set signal to handle HUP - abort\n");
		exit(8);
	}
	signal_info.sa_handler=logsurfer_exit;
	if ( sigaction(SIGTERM, &signal_info, NULL) ) {
		(void) fprintf(stderr, "can't set signal to handle TERM - abort\n");
		exit(8);
	}
	if ( sigaction(SIGINT, &signal_info, NULL) ) {
		(void) fprintf(stderr, "can't set signal to handle INT - abort\n");
		exit(8);
	}

	while ( logfile != NULL ) {
		logline=readline(logfile, &logline_buffer, &logline_buffer_size,
			&logline_buffer_pos);
		(void) time(&current_time);
		if ( logline != NULL ) {
			logline_num++;
			process_logline();
			(void) free(logline);
		}
		else if ( do_follow ) {
			(void) sleep(1);
            if ( do_follow == 2 && fileno(logfile) != STDIN_FILENO
                && stat(logfile_name, &nstb) != -1 ) {
                    if ( nstb.st_ino != ostb.st_ino || 
                         nstb.st_dev != ostb.st_dev ||
                         nstb.st_rdev != ostb.st_rdev ||
                         nstb.st_nlink == 0 ) {
                        logfile_reopen(SIGHUP);
                        (void) memcpy(&ostb, &nstb, sizeof(struct stat));
                    }
            }
        }
		else {
			(void) fclose(logfile);
			logfile=NULL;
		}
		regex_submatches_num=0;
		if ( next_rule_timeout < (long) current_time )
			check_rule_timeout();
		if ( next_context_timeout < (long) current_time )
			check_context_timeout();
		check_context_linelimit();
#ifdef SENDMAIL_FLUSH
		if ( flush_time != LONG_MAX ) {
			if ( flush_time < (long) current_time ) {
#ifdef DEBUG
(void) printf("time to flush the sendmail queue now\n");
#endif
				flush_queue();
			}
#ifdef DEBUG
			else
(void) printf("next flush time in %ld seconds\n", flush_time-((long) current_time));
#endif
		}
#endif
		while ( waitpid((pid_t) -1, NULL, WNOHANG) > 0 )
			;
	}
	logsurfer_exit(0);

	/* NOTREACHED */
	return(99);
}

