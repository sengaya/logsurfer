/*
 * globals.h
 *
 * definition of global variables of the logsurfer package
 *
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdio.h>

#if HAVE_LIMITS_H
#include <limits.h>
#endif

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
#include "regex.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

extern struct rule	*all_rules;	/* all active rules		*/
extern struct rule	*all_rules_end;
extern struct context	*all_contexts;	/* all active contexts		*/
extern struct context	*all_contexts_end;
extern long		next_rule_timeout;	/* next rule timeout	*/
extern long		next_context_timeout;	/* next context timeout	*/

extern char		logfile_name[MAXPATHLEN];	/* name of the logfile	*/
extern FILE		*logfile;		/* input logfile	*/
extern long		logline_num;	/* linenumer of the logline	*/
extern char		*logline;	/* the actual logline		*/
extern struct context_line *logline_context;/* the context_line ptr	*/
extern time_t		current_time;	/* current time information	*/

extern char		dumpfile_name[MAXPATHLEN];       /* name of dumpfile */

extern int		exit_silent;	/* exit silently?		*/

extern int              timeout_contexts_at_exit;     /* timeout contexts when exit? */


/*
 * definitions for the regular expression matching routines
 *
 * we have an array with submatches and the number of valid submatches
 */
extern struct re_registers regex_submatches;
extern struct re_registers regex_notmatches;
extern int 		regex_submatches_num;

/*
 * new argv and argc for the externals programs we are starting
 */
extern char		*new_argv[256];	/* build a new argv[] list 	*/
extern int		new_argc;


#ifdef SENDMAIL_FLUSH
/*
 * should we flush the mail-queue?
 */
extern long		flush_time;

#endif	/* SENDMAIL_FLUSH */

#endif		/* GLOBALS_H */

