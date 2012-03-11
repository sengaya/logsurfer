/*
 * exec.c
 *
 * routines to invoke externale programs
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
#include <sys/types.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_MALLOC_H
#include <malloc.h>
#endif

#include <syslog.h>


/* local includes */

#include "logsurfer.h"
#include "globals.h"
#include "str_util.h"
#include "exec.h"


/*
 * prepare a command for exec (split the string for argv[])
 */
int
prepare_exec(cmdstring)
	char	*cmdstring;
{
	int	old_matchnum;
	char	*src;

#ifdef DEBUG
(void) printf("preparing for exec: %s\n", cmdstring);
#endif
	/* cleanup the old new_argv */
	while ( new_argc >= 0 ) {
		if ( new_argv[new_argc] != NULL )
			(void) free(new_argv[new_argc]);
		new_argc--;
	}
	new_argc=0;

	src=cmdstring;
	/* suppress expanding of $var in the command line (already done) */
	old_matchnum=regex_submatches_num;
	while ( (new_argc < 255) && ((new_argv[new_argc++]=get_word(&src)) != NULL) )
		;
	regex_submatches_num=old_matchnum;
	if ( (new_argc == 255) || (*(skip_spaces(src)) != '\0') ){
        new_argc--;
		return(0);
    }
    if ( (new_argv[new_argc]=(char *)malloc(sizeof(char))) == NULL ){
        new_argc--;
        return(0);
    }
    *new_argv[new_argc]='\0';
	return(1);
}


/*
 * exec an external command
 */
void
do_exec(exec_tokens)
	struct action_tokens	*exec_tokens;
{
	char	*cmdstring;

	if ((cmdstring=exec_tokens->this_word) == NULL ) {
		(void) fprintf(stderr, "exec definition without program\n");
		return;
	}

	if ( prepare_exec(cmdstring) == 0 ) {
		(void) fprintf(stderr, "can't parse cmd: %s\n", cmdstring);
		return;
	}

#ifdef SENDMAIL_FLUSH
#ifdef DEBUG
(void) printf("setting flush_time to %d seconds\n", FLUSH_DELAY);
#endif
	flush_time=((long) current_time)+FLUSH_DELAY;
#endif
	switch ( fork() ) {
	case -1:
		(void) fprintf(stderr, "can't fork for cmd: %s\n", cmdstring);
		return;
	case 0:
		if ( execv(new_argv[0], new_argv) )
			(void) fprintf(stderr, "can't exec cmd: %s\n", cmdstring);
		return;
	default:
		/*
		 * here we may want to wait for the forked program...
		 * but waiting will block analyzing logdata so we don't wait...
		 */
		;
	}

	return;
}


/*
 * pipe the logline thrugh an external command
 */
void
do_pipe_line(cmd_token)
	struct action_tokens	*cmd_token;
{
	char	*cmdstring;
	int	filedes[2];		/* the file descriptors for pipe()	*/

	if ( (cmdstring=cmd_token->this_word) == NULL ) {
		(void) fprintf(stderr, "start pipe without command\n");
		return;
	}
#ifdef DEBUG
(void) printf("setting up pipe to command: %s\n", cmdstring);
#endif

	if ( prepare_exec(cmdstring) == 0 ) {
		(void) fprintf(stderr, "can't parse cmd: %s\n", cmdstring);
		return;
	}

	if ( pipe(filedes) != 0 ) {
		(void) fprintf(stderr, "can't open pipe for cmd: %s\n", cmdstring);
		return;
	}
	switch ( fork() ) {
	case -1:
		(void) fprintf(stderr, "can't fork for cmd: %s\n", cmdstring);
		return;
	case 0:
		(void) close(filedes[1]);
		(void) dup2(filedes[0], 0);
		if ( execv(new_argv[0], new_argv) )
			(void) fprintf(stderr, "can't exec cmd: %s\n", cmdstring);
		return;
	default:
		(void) close(filedes[0]);
		(void) write(filedes[1], logline, strlen(logline));
		(void) write(filedes[1], "\n", 1);
		(void) close(filedes[1]);
		/*
		 * here we may want to wait for the forked program...
		 * but waiting will block analyzing logdata so we don't wait...
		 */
#ifdef SENDMAIL_FLUSH
#ifdef DEBUG
(void) printf("setting flush_time to %d seconds\n", FLUSH_DELAY);
#endif
		flush_time=((long) current_time)+FLUSH_DELAY;
#endif
	}

	return;
}


/*
 * collec tokens from a default context action
 */
struct action_tokens *
collect_tokens(src)
	char	*src;
{
	char			*help_ptr;
	struct action_tokens	*new_token, *result, *last_token;

	result=last_token=NULL;
	while (*src != '\0') {
		if ((new_token=(struct action_tokens *)
			malloc(sizeof(struct action_tokens))) == NULL ) {
			/* out of memory - cleanup and exit */
			free_tokens(result);
			return(NULL);
		}
		if ( (help_ptr=get_word(&src)) == NULL ) {
			free_tokens(result);
			return(NULL);
		}
		new_token->this_word=help_ptr;
		new_token->next=NULL;
		if ( last_token == NULL ) {
			result=new_token;
			last_token=result;
		}
		else {
			last_token->next=new_token;
			last_token=new_token;
		}
		src=skip_spaces(src);
	}
	return(result);
}


/*
 * free a token list
 */
void
free_tokens(token_list)
	struct action_tokens	*token_list;
{
	struct action_tokens	*next_token;

	while (token_list != NULL) {
		next_token=token_list->next;
		if ( token_list->this_word != NULL )
			(void) free(token_list->this_word);
		(void) free(token_list);
		token_list=next_token;
	}
	return;
}


#ifdef SENDMAIL_FLUSH

/*
 * flush the sendmail-queue
 *
 * ...this is an odd hack and I don't like it
 */
void
flush_queue()
{
	switch ( fork() ) {
	case -1:
		(void) fprintf(stderr, "can't fork for flushing the sendmail queue\n");
		/* is it a godd or a bad idea to try it again? */
		flush_time+=FLUSH_DELAY*10;
		return;
	case 0:
		if ( execl("/usr/lib/sendmail", "sendmail", "-q", (char *)NULL) )
			(void) fprintf(stderr, "can't exec sendmail-flush\n");
		return;
	default:
		/*
		 * here we may want to wait for the forked program...
		 * but waiting will block analyzing logdata so we don't wait...
		 */
#ifdef DEBUG
(void) printf("resetting flush_time\n");
#endif
		flush_time=LONG_MAX;
	}

	return;
}

#endif	/* SENDMAIL_FLUSH */


/*
 * echo the tokens to stdout
 */
void
do_echo(echo_tokens)
	struct action_tokens	*echo_tokens;
{
	char	*echostring;
	char *outfile_name;
	FILE *outfile;
	char *write_mode;

	if ((echostring=echo_tokens->this_word) == NULL ) {
		(void) fprintf(stderr, "echo definition without argument\n");
		return;
	}
	if( echostring[0] == '>' ){
	  /* we've got a filename : grab it, open it, and hop to the next string */
	  outfile_name = echostring+1;
	  if( outfile_name[0] == '>' ){
	    outfile_name++;
	    write_mode = "a"; /* append */
	  } else {
	    write_mode = "w";
	  }
	  if (echo_tokens->next == NULL || (echostring=(echo_tokens->next)->this_word) == NULL ) {
	    (void) fprintf(stderr, "echo definition without argument\n");
	    return;
	  }
      echo_tokens = echo_tokens->next;
	  outfile = fopen( outfile_name, write_mode );
	  if( outfile == NULL ){
	    (void) fprintf(stderr, "failed to open echo output file %s\n", outfile_name );
	    return;
	  }
	} else {
        /* normal stdout output */
        outfile = stdout;
	}

    fprintf(outfile,"%s\n",echostring);
    if( outfile == stdout )
        fflush(stdout);
    else
        fclose(outfile);

	return;
}

void
do_syslog(syslog_tokens)
struct action_tokens    *syslog_tokens;
{
	char *fac_level_str;
	char *log_str;
	int syslog_facility, syslog_level;

	fac_level_str = syslog_tokens->this_word;
	if(syslog_tokens->next == NULL || (log_str=(syslog_tokens->next)->this_word) == NULL ){
		(void) fprintf(stderr, "syslog command without enough arguments");
		return;
	}

	/* parse facility & level */
	if( !parse_syslog_faclevel( fac_level_str, &syslog_facility, &syslog_level ) ){
		(void) fprintf(stderr, "unknown syslog facility/level argument");
		return;
	}

	/* open syslog */
	openlog( "logsurfer", LOG_NDELAY|LOG_PID, syslog_facility );
	syslog( syslog_level, "%s", log_str=(syslog_tokens->next)->this_word );
	closelog();
}

/*
  Parse syslog facility:level parameter string
  returns 0-failed, 1-success
*/
int parse_syslog_faclevel( faclevel, facility, level )
char *faclevel;
int *facility, *level;
{
	char *levelstr;

	if( (levelstr = strchr(faclevel,':')) == NULL){
		(void) fprintf(stderr, "no colon in facility:level argument" );  /*debug*/
		return 0;
	}
	levelstr++;

	if( !strncmp(faclevel,"kern",4) )
		*facility = LOG_KERN;
#ifdef LOG_AUTHPRIV
	else if( !strncmp(faclevel,"authpriv",8) )
		*facility = LOG_AUTHPRIV;
#endif
#ifdef LOG_CRON
	else if( !strncmp(faclevel,"cron",4) )
		*facility = LOG_CRON;
#endif
	else if( !strncmp(faclevel,"daemon",6) )
		*facility = LOG_DAEMON;
#ifdef LOG_FTP
	else if( !strncmp(faclevel,"ftp",3) )
		*facility = LOG_FTP;
#endif
#ifdef LOG_AUTH
	else if( !strncmp(faclevel,"auth",4) )
		*facility = LOG_AUTH;
#endif
	else if( !strncmp(faclevel,"local0",6) )
		*facility = LOG_LOCAL0;
	else if( !strncmp(faclevel,"local1",6) )
		*facility = LOG_LOCAL1;
	else if( !strncmp(faclevel,"local2",6) )
		*facility = LOG_LOCAL2;
	else if( !strncmp(faclevel,"local3",6) )
		*facility = LOG_LOCAL3;
	else if( !strncmp(faclevel,"local4",6) )
		*facility = LOG_LOCAL4;
	else if( !strncmp(faclevel,"local5",6) )
		*facility = LOG_LOCAL5;
	else if( !strncmp(faclevel,"local6",6) )
		*facility = LOG_LOCAL6;
	else if( !strncmp(faclevel,"local7",6) )
		*facility = LOG_LOCAL7;
	else if( !strncmp(faclevel,"lpr",3) )
		*facility = LOG_LPR;
	else if( !strncmp(faclevel,"mail",4) )
		*facility = LOG_MAIL;
#ifdef LOG_NEWS
	else if( !strncmp(faclevel,"news",4) )
		*facility = LOG_NEWS;
#endif
	else if( !strncmp(faclevel,"syslog",6) )
		*facility = LOG_SYSLOG;
	else if( !strncmp(faclevel,"user",4) )
		*facility = LOG_USER;
#ifdef LOG_UUCP
	else if( !strncmp(faclevel,"uucp",4) )
		*facility = LOG_UUCP;
#endif
	else {
		/* facility unknown */
		(void) fprintf(stderr, "unknown facility" );  /*debug*/
		return 0;
	}

	if( !strncmp(levelstr,"emerg",5) )
		*level = LOG_EMERG;
	else if( !strncmp(levelstr,"alert",5) )
		*level = LOG_ALERT;
	else if( !strncmp(levelstr,"crit",4) )
		*level = LOG_CRIT;
	else if( !strncmp(levelstr,"err",3) )
		*level = LOG_ERR;
	else if( !strncmp(levelstr,"warn",4) )
		*level = LOG_WARNING;
	else if( !strncmp(levelstr,"notice",6) )
		*level = LOG_NOTICE;
	else if( !strncmp(levelstr,"info",4) )
		*level = LOG_INFO;
	else if( !strncmp(levelstr,"debug",5) )
		*level = LOG_DEBUG;
	else {
		/* level unknown */
		(void) fprintf(stderr, "unknown level" );  /*debug*/
		return 0;
	}

	return 1;
}




