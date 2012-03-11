/*
 * exit.c
 *
 * routines to exit the logsurfer, dump the database and to cleanup
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

#include <sys/types.h>

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


/* local includes */

#include "logsurfer.h"
#include "globals.h"
#include "regex.h"
#include "str_util.h"
#include "rule.h"
#include "context.h"
#include "exit.h"


/*
 * dump the internal data to the debug file...
 */
void
real_dump_data()
{
	struct rule		*rule_ptr;
	struct context		*context_ptr;
	struct context_body	*context_body_ptr;
	struct action_tokens	*this_token, *next_token;
	FILE			*dumpfile;

	if ( (dumpfile=fopen(dumpfile_name, "w")) == NULL ) {
		(void) fprintf(stderr, "can't open dumpfile - no dump today\n");
		return;
	}

	(void) time(&current_time);
	(void) fprintf(dumpfile, "**** dumpfile of logsurfer from %s\n", ctime(&current_time));
	(void) fprintf(dumpfile, "*******************************************************\n");
	(void) fprintf(dumpfile, "* RULES\n");
	(void) fprintf(dumpfile, "*\n");
	(void) fprintf(dumpfile, "*     match_regex match_not_regex\n");
	(void) fprintf(dumpfile, "*           stop_regex stop_not_regex timeout [continue]\n");
	(void) fprintf(dumpfile, "*           action\n");
	(void) fprintf(dumpfile, "*******************************************************\n\n");
	for (rule_ptr=all_rules; rule_ptr != NULL; rule_ptr=rule_ptr->next) {
		(void) fprintf(dumpfile, "-----\n");
		(void) fprintf(dumpfile, "\"%s\" ", rule_ptr->match_regex_str);
		if ( rule_ptr->match_not_regex_str == NULL )
			(void) fprintf(dumpfile, "-\n");
		else
			(void) fprintf(dumpfile, "\"%s\"\n", rule_ptr->match_not_regex_str);
		if ( rule_ptr->stop_regex_str == NULL )
			(void) fprintf(dumpfile, "\t- ");
		else
			(void) fprintf(dumpfile, "\t\"%s\" ", rule_ptr->stop_regex_str);
		if ( rule_ptr->stop_not_regex_str == NULL )
			(void) fprintf(dumpfile, "- ");
		else
			(void) fprintf(dumpfile, "\"%s\" ", rule_ptr->stop_not_regex_str);
		if ( rule_ptr->timeout == LONG_MAX )
			(void) fprintf(dumpfile, "0");
		else
			(void) fprintf(dumpfile, "%ld", rule_ptr->timeout-(long)current_time);
		if ( rule_ptr->do_continue == 1 )
			(void) fprintf(dumpfile, " CONTINUE");
		(void) fprintf(dumpfile, "\n\t");
		switch ( rule_ptr->action_type ) {
		case ACTION_IGNORE:
			(void) fprintf(dumpfile, "IGNORE\n");
			break;
		case ACTION_EXEC:
			(void) fprintf(dumpfile, "EXEC %s\n", rule_ptr->action_body);
			break;
		case ACTION_PIPE:
			(void) fprintf(dumpfile, "PIPE %s\n", rule_ptr->action_body);
			break;
		case ACTION_OPEN:
			(void) fprintf(dumpfile, "OPEN %s\n", rule_ptr->action_body);
			break;
		case ACTION_DELETE:
			(void) fprintf(dumpfile, "DELETE %s\n", rule_ptr->action_body);
			break;
		case ACTION_REPORT:
			(void) fprintf(dumpfile, "REPORT %s\n", rule_ptr->action_body);
			break;
		case ACTION_RULE:
			(void) fprintf(dumpfile, "RULE %s\n", rule_ptr->action_body);
			break;
		case ACTION_ECHO:
			(void) fprintf(dumpfile, "ECHO %s\n", rule_ptr->action_body);
			break;
		case ACTION_SYSLOG:
			(void) fprintf(dumpfile, "SYSLOG %s\n", rule_ptr->action_body);
			break;
		default:
			(void) fprintf(dumpfile, "(UNKNOWN) - shouldn't happen\n");
		}
	}
	(void) fprintf(dumpfile, "\n\n");

	(void) fprintf(dumpfile, "*******************************************************\n");
	(void) fprintf(dumpfile, "* CONTEXT Information\n");
	(void) fprintf(dumpfile, "*\n");
	(void) fprintf(dumpfile, "*     match_regex match_not_regex\n");
	(void) fprintf(dumpfile, "*           max_lines timeout_abs timeout_rel min_lines action\n");
	(void) fprintf(dumpfile, "*           contents\n");
	(void) fprintf(dumpfile, "*******************************************************\n\n");
	for ( context_ptr=all_contexts; context_ptr!=NULL; context_ptr=context_ptr->next ) {
		(void) fprintf(dumpfile, "-----\n");
		(void) fprintf(dumpfile, "\"%s\" ", context_ptr->match_regex_str);
		if ( context_ptr->match_not_regex_str == NULL )
			(void) fprintf(dumpfile, "-\n");
		else
			(void) fprintf(dumpfile, "\"%s\"\n", context_ptr->match_not_regex_str);
		if ( context_ptr->max_lines == LONG_MAX )
			(void) fprintf(dumpfile, "\t0 ");
		else
			(void) fprintf(dumpfile, "\t%ld ", context_ptr->max_lines);
		if ( context_ptr->timeout_abs == LONG_MAX )
			(void) fprintf(dumpfile, "0 ");
		else
			(void) fprintf(dumpfile, "%ld ", context_ptr->timeout_abs);
		if ( context_ptr->timeout_rel_offset == LONG_MAX )
			(void) fprintf(dumpfile, "0 ");
		else
			(void) fprintf(dumpfile, "%ld ", context_ptr->timeout_rel_offset);
		(void) fprintf(dumpfile, "%ld ", context_ptr->min_lines);
		switch (context_ptr->action_type) {
		case ACTION_IGNORE:
			(void) fprintf(dumpfile, "IGNORE");
			break;
		case ACTION_EXEC:
			(void) fprintf(dumpfile, "EXEC");
			break;
		case ACTION_PIPE:
			(void) fprintf(dumpfile, "PIPE");
			break;
		case ACTION_REPORT:
			(void) fprintf(dumpfile, "REPORT");
			break;
		case ACTION_ECHO:
			(void) fprintf(dumpfile, "ECHO");
			break;
		case ACTION_SYSLOG:
			(void) fprintf(dumpfile, "SYSLOG");
			break;
		default:
			(void) fprintf(dumpfile, "(UNKNOWN) - shouldn't happen");
		}
		this_token=context_ptr->action_tokens;
		while (this_token != NULL ) {
			next_token=this_token->next;
			(void) fprintf(dumpfile, " %s", this_token->this_word);
			this_token=next_token;
		}
		(void) fprintf(dumpfile, "\n");
		for ( context_body_ptr=context_ptr->body; context_body_ptr != NULL;
			context_body_ptr=context_body_ptr->next ) {
			(void) fprintf(dumpfile, "\t%8ld: %s\n",
				context_body_ptr->this_line->linenumber,
				context_body_ptr->this_line->content);
		}
	}
	(void) fprintf(dumpfile, "\n\n");

	(void) fprintf(dumpfile, "**** dumpfile end\n");

	(void) fclose(dumpfile);
	return;
}


/*
 * catch the dump signal, write a message and dump the state
 */
void
dump_data()
{
	(void) fprintf(stderr, "dumping state to %s\n", dumpfile_name);
	real_dump_data();
	return;
}


/*
 * cleanup the alloced() memory --- should only be done at exit time
 */
void
cleanup_memory()
{
	struct context	*this_context, *next_context;
	struct rule	*this_rule, *next_rule;

	/* first free the contexts */
	this_context=all_contexts;
	while ( this_context != NULL ) {
		next_context=this_context->next;
		unlink_context(this_context);
		this_context=next_context;
	}

	/* now purge the rules that are still in memory */
	this_rule=all_rules;
	while ( this_rule != NULL ) {
		next_rule=this_rule->next;
		unlink_rule(this_rule);
		this_rule=next_rule;
	}

	/* cleanup the old new_argv */
	while ( new_argc >= 0 ) {
		if ( new_argv[new_argc] != NULL )
			(void) free(new_argv[new_argc]);
		new_argc--;
	}
	new_argc=0;

	/* now there are only those parts from main() left... */
	(void) free(regex_submatches.start);
	(void) free(regex_submatches.end);
	(void) free(regex_notmatches.start);
	(void) free(regex_notmatches.end);

	/*
	 * unfourtunatly will still may have some malloc()ed memory...
	 *
	 * the reason for the remaining memory part is quite simple: we don't
	 * know where we received the signal to quit and don't know which
	 * part are malloced while doing the current action (for example
	 * processing a logline
	 */

	return;
}


/*
 * exit the program...
 */
void
logsurfer_exit(sig)
	int	sig;
/* ARGSUSED */
{
	struct context	*this_context, *next_context;
	int		child_retry;

	if (!exit_silent)
		(void) fprintf(stderr, "exiting program - please wait...\n");

	/* dump the data to disk */
	/* do this before sending timeout or we may have no contexts left */
	if (exit_silent)
		real_dump_data();
	else
		dump_data();

	/* check for timeouts */
	if (!exit_silent)
		(void) fprintf(stderr, "sending timeout to contexts...\n");
	this_context=all_contexts;
	while ( this_context != NULL ) {
		next_context=this_context->next;
		if ( (this_context->timeout_abs != LONG_MAX) ||
			(this_context->timeout_rel != LONG_MAX) ) {
		        if( timeout_contexts_at_exit )
			  do_context_action(this_context);
			unlink_context(this_context);
		}
		this_context=next_context;
	}

	/* free the remaining contexts and rules */
	if (!exit_silent)
		(void) fprintf(stderr, "cleaning up memory...\n");
	cleanup_memory();

	/* wait for childs */
	child_retry=0;
	while ( (waitpid((pid_t) -1, NULL, WNOHANG) >= 0) && (child_retry < 4) ) {
		if (!exit_silent)
			(void) fprintf(stderr,
				"waiting for childs (retry #%d)...\n", ++child_retry);
		(void) sleep(5);
		while (waitpid((pid_t) -1, NULL, WNOHANG) > 0)
			;
	}

	if ( logfile != NULL )
		(void) fclose(logfile);

	/* what exit value should we use? */
	exit(0);
}


