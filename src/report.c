/*
 * report.c
 *
 * routines to handle reports (create, collect, ...)
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

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_MALLOC_H
#include <malloc.h>
#endif


/* local includes */

#include "logsurfer.h"
#include "globals.h"
#include "str_util.h"
#include "context.h"
#include "exec.h"
#include "report.h"



/*
 * generate a report witha summary of contexts and pipe it through an
 * external command
 */
void
make_report(action_body, ref_context_body)
	struct action_tokens	*action_body;
	struct context_body	*ref_context_body;
{
	char			*cmdstring;
	struct context		*next_context;
	struct context_body	report_body;
	struct context_body	*out_body;
	struct context_line	dummy_line;
	struct action_tokens	*this_word;
	int			filedes[2];

	if ( (cmdstring=action_body->this_word) == NULL ) {
		(void) fprintf(stderr, "report without commands\n");
		return;
	}
	if ( prepare_exec(cmdstring) == 0 ) {
		(void) fprintf(stderr, "can't parse cmd: %s\n", cmdstring);
		return;
	}

	dummy_line.linenumber=0;
	(void) time(&current_time);
	dummy_line.timestamp=(long) current_time;
	dummy_line.content=NULL;
	dummy_line.link_counter=1;
	report_body.this_line=&dummy_line;
	report_body.next=NULL;

	this_word=action_body->next;
	while (this_word != NULL) {
#ifdef DEBUG
(void) printf("searching for context: ***%s***\n", this_word->this_word);
#endif
		if ( !strcmp(this_word->this_word,"-") && ref_context_body != NULL )
			add_to_report(&report_body, ref_context_body);
		else if ( (next_context=find_context(this_word->this_word)) != NULL )
			add_to_report(&report_body, next_context->body);
		this_word=this_word->next;
	}

	if ( pipe(filedes) != 0 ) {
		(void) fprintf(stderr, "can't open pipe for report: %s\n", cmdstring);
		destroy_body(report_body.next);
		return;
	}
	switch ( fork() ) {
	case -1:
		(void) fprintf(stderr, "can't fork for report: %s\n", cmdstring);
		destroy_body(report_body.next);
		return;
	case 0:
		(void) close(filedes[1]);
		(void) dup2(filedes[0], 0);
		if ( execv(new_argv[0], new_argv) )
			(void) fprintf(stderr, "can't exec reprt-cmd: %s\n", cmdstring);
		/*
		 * is this a task of the client? ...don't think so
		 */
		/* destroy_body(report_body.next); */

		return;
	default:
		(void) close(filedes[0]);
		/*
		 * now write the contents of the collected contexts
		 */
		out_body=report_body.next;
		while ( out_body != NULL ) {
			if ( out_body->this_line->content != NULL ) {
				(void) write(filedes[1], out_body->this_line->content,
					strlen(out_body->this_line->content));
				(void) write(filedes[1], "\n", 1);
			}
			out_body=out_body->next;
		}
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

	destroy_body(report_body.next);
	return;
}


/*
 * add a context to a report
 */
void
add_to_report(report_body, new_body)
	struct context_body	*report_body;
	struct context_body	*new_body;
{
	struct context_body	*src, *dst, *prev, *new_context_line;

#ifdef DEBUG
(void) printf("adding body to report, first line is: ***%s***\n", new_body->this_line->content);
#endif
	dst=report_body;
	prev=NULL;
	for ( src=new_body; src != NULL; src=src->next ) {
		while ( (dst != NULL ) && 
			(dst->this_line->linenumber < src->this_line->linenumber) ) {
			prev=dst;
			dst=dst->next;
		}
		if ( (dst == NULL) ||
			(dst->this_line->linenumber != src->this_line->linenumber) ) {
			if ( (new_context_line=(struct context_body *)
				malloc(sizeof(struct context_body))) == NULL ) {
				(void) fprintf(stderr,
					"out of memory collecting report\n");
				return;
			}
#ifdef DEBUG
(void) printf("adding line to report: ***%s***\n", src->this_line->content);
#endif
			new_context_line->this_line=src->this_line;
			new_context_line->this_line->link_counter++;
			new_context_line->next=dst;
			prev->next=new_context_line;
			prev=new_context_line;
		}
	}

	return;
}


