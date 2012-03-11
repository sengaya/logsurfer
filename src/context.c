/*
 * context.c
 *
 * routines to handle contexts (create, delete, ...)
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

#if HAVE_MALLOC_H
#include <malloc.h>
#endif


/* local includes */

#include "logsurfer.h"
#include "globals.h"
#include "regex.h"
#include "str_util.h"
#include "report.h"
#include "exec.h"
#include "context.h"


/*
 * create a new (empty) context
 */
struct context *
create_context()
{
	struct context	*new_context;

	if ( (new_context=(struct context *)malloc(sizeof(struct context))) == NULL )
		return NULL;

	new_context->match_regex_str=NULL;
	new_context->match_regex=NULL;
	new_context->match_not_regex_str=NULL;
	new_context->match_not_regex=NULL;
	new_context->max_lines=LONG_MAX;
	new_context->min_lines=0;
	new_context->timeout_abs=LONG_MAX;
	new_context->timeout_rel=LONG_MAX;
	new_context->timeout_rel_offset=0;
	new_context->action_type=ACTION_UNKNOWN;
	new_context->action_tokens=NULL;
	new_context->body=NULL;
	new_context->lines=0;
	new_context->last=NULL;
	new_context->next=NULL;
	new_context->previous=NULL;

	return(new_context);
}


/*
 * open a new context and add the logline to he context
 */
void 
open_context(context_def)
	char	*context_def;
{
	struct context		*new_context;
	char			*src;

#ifdef DEBUG
(void) printf("opening new context: ***%s***\n", context_def);
#endif

	if ( (new_context=create_context()) == NULL ) {
		(void) fprintf(stderr, "out of memory creating new context: %s\n",
			context_def);
		return;
	}
	src=context_def;

	/* get the match_regex_str */
	if ( (new_context->match_regex_str=get_word(&src)) == NULL ) {
		(void) fprintf(stderr, "can't get match_regex for: %s\n",
			context_def);
		destroy_context(new_context);
		return;
	}
	/* check if the context is already existing... */
	if ( find_context(new_context->match_regex_str) != NULL ) {
#ifdef DEBUG
(void) printf("context already exists: ***%s***\n", new_context->match_regex_str);
#endif
		destroy_context(new_context);
		return;
	}

	/* get the match_regex */
	if ( (new_context->match_regex=(struct re_pattern_buffer *)
		malloc(sizeof(struct re_pattern_buffer))) == NULL ) {
		(void) fprintf(stderr, "out of memory opening context: %s\n", context_def);
		destroy_context(new_context);
		return;
	}
	/* use fastmap if enough memeory available */
	new_context->match_regex->fastmap=(char *)malloc(256);
	new_context->match_regex->translate=(char *) 0;
	new_context->match_regex->buffer=NULL;
	new_context->match_regex->allocated=0;
	if ( re_compile_pattern(new_context->match_regex_str, strlen(new_context->match_regex_str),
		new_context->match_regex) != 0 ) {
		(void) fprintf(stderr, "can't parse match_regex for: %s\n", context_def);
		destroy_context(new_context);
		return;
	}
	new_context->match_regex->regs_allocated=REGS_FIXED;

	/* get the optional match_not_regex (the expresion that should *not* match) */
	src=skip_spaces(src);
	if ( *src != '-' ) {
		if ( (new_context->match_not_regex_str=get_word(&src)) == NULL ) {
			(void) fprintf(stderr, "error in match_not_regex of context: %s\n",
				context_def);
			destroy_context(new_context);
			return;
		}
		if ( (new_context->match_not_regex=(struct re_pattern_buffer *)
			malloc(sizeof(struct re_pattern_buffer))) == NULL ) {
			(void) fprintf(stderr, "out of memory adding context: %s\n",
				context_def);
			destroy_context(new_context);
			return;
		}
		/* use fastmap if enough memeory available */
		new_context->match_not_regex->fastmap=(char *)malloc(256);
		new_context->match_not_regex->translate=(char *) 0;
		new_context->match_not_regex->buffer=NULL;
		new_context->match_not_regex->allocated=0;
		if ( re_compile_pattern(new_context->match_not_regex_str,
			strlen(new_context->match_not_regex_str),
			new_context->match_not_regex) != 0 ) {
			(void) fprintf(stderr, "error in match_not_regex of context: %s\n",
				context_def);
			destroy_context(new_context);
			return;
		}
		new_context->match_not_regex->regs_allocated=REGS_FIXED;
	}
	else
		src++;


	/* get the maximum number of lines */
	src=skip_spaces(src);
	new_context->max_lines=atol(src);
	while ( (*src != '\0') && (!isspace(*src)) )
		src++;
	if ( new_context->max_lines == 0 )
		new_context->max_lines=LONG_MAX;

	/* get the absolut timeout */
	src=skip_spaces(src);
	new_context->timeout_abs=atol(src);
	while ( (*src != '\0') && (!isspace(*src)) )
		src++;
	if ( new_context->timeout_abs == 0 )
		new_context->timeout_abs=LONG_MAX;
	else {
		new_context->timeout_abs+=(long) current_time;
		if ( new_context->timeout_abs < next_context_timeout )
			next_context_timeout=new_context->timeout_abs;
	}

	/* get the relative timeout */
	src=skip_spaces(src);
	new_context->timeout_rel_offset=atol(src);
	while ( (*src != '\0') && (!isspace(*src)) )
		src++;
	if ( new_context->timeout_rel_offset == 0 )
		new_context->timeout_rel=LONG_MAX;
	else {
		new_context->timeout_rel+=(long) current_time;
		if ( new_context->timeout_rel < next_context_timeout )
			next_context_timeout=new_context->timeout_rel;
	}

	/* if the next field is numeric, then its an optional min_lines */
	src=skip_spaces(src);
	if( isdigit(*src) ){
	  new_context->min_lines=atol(src);
	  while ( (*src != '\0') && (!isspace(*src)) )
	    src++;
	}

	/* the rest is the default action */
	src=skip_spaces(src);
	if ( !strncasecmp(src, "ignore", 6) ) {
		new_context->action_type=ACTION_IGNORE;
	}
	else if ( !strncasecmp(src, "exec", 4) ) {
		new_context->action_type=ACTION_EXEC;
		src=skip_spaces(src+4);
		if ( (new_context->action_tokens=collect_tokens(src)) == NULL ) {
			(void) fprintf(stderr, "out of memory creating context: %s\n",
				context_def);
			destroy_context(new_context);
			return;
		}
	}
	else if ( !strncasecmp(src, "pipe", 4) ) {
		new_context->action_type=ACTION_PIPE;
		src=skip_spaces(src+4);
		if ( (new_context->action_tokens=collect_tokens(src)) == NULL ) {
			(void) fprintf(stderr, "out of memory creating context: %s\n",
				context_def);
			destroy_context(new_context);
			return;
		}
	}
	else if ( !strncasecmp(src, "report", 6) ) {
		new_context->action_type=ACTION_REPORT;
		src=skip_spaces(src+6);
		if ( (new_context->action_tokens=collect_tokens(src)) == NULL ) {
			(void) fprintf(stderr, "out of memory creating context: %s\n",
				context_def);
			destroy_context(new_context);
			return;
		}
	}
	else if ( !strncasecmp(src, "echo", 4) ) {
	        new_context->action_type=ACTION_ECHO;
		src=skip_spaces(src+4);
		if ( (new_context->action_tokens=collect_tokens(src)) == NULL ) {
			(void) fprintf(stderr, "out of memory creating context: %s\n",
				context_def);
			destroy_context(new_context);
			return;
		}
	}
	else if ( !strncasecmp(src, "syslog", 6) ) {
	        new_context->action_type=ACTION_SYSLOG;
		src=skip_spaces(src+6);
		if ( (new_context->action_tokens=collect_tokens(src)) == NULL ) {
			(void) fprintf(stderr, "out of memory creating context: %s\n",
				context_def);
			destroy_context(new_context);
			return;
		}
	}
	else {
		(void) fprintf(stderr, "unknown default action: %s\n", src);
		destroy_context(new_context);
		return;
	}

	/* add the current logline to the context */
	add_to_context(new_context, logline_context);

	/* link the nex context into the global list */
	if ( all_contexts_end == NULL ) {
		all_contexts=new_context;
		all_contexts_end=new_context;
	}
	else {
		all_contexts_end->next=new_context;
		new_context->previous=all_contexts_end;
		all_contexts_end=new_context;
	}
	return;
}


/*
 * destroy (free) a context line
 */
void
destroy_context_line(this_context_line)
	struct context_line	*this_context_line;
{
	if ( this_context_line == NULL )
		return;
	/*
	 * are we allowed to free the context line?
	 */
	if ( this_context_line->link_counter != 0 )
		return;
	if ( this_context_line->content != NULL )
		(void) free(this_context_line->content);
	(void) free(this_context_line);
	return;
}


/*
 * destroy (free) a linked body list
 */
void
destroy_body(this_body)
	struct context_body	*this_body;
{
	struct context_body	*help_ptr;
	struct context_body	*next_ptr;

	next_ptr=NULL;
	for ( help_ptr=this_body; help_ptr != NULL; help_ptr=next_ptr ) {
		next_ptr=help_ptr->next;
		if ( --help_ptr->this_line->link_counter == 0 )
			destroy_context_line(help_ptr->this_line);
		(void) free(help_ptr);
	}
	return;
}


/*
 * destroy (free) a context
 */
void
destroy_context(this_context)
	struct context	*this_context;
{
	destroy_body(this_context->body);
	if ( this_context->match_regex_str != NULL )
		(void) free(this_context->match_regex_str);
	if ( this_context->match_regex != NULL ) {
		regfree(this_context->match_regex);
		(void) free(this_context->match_regex);
	}
	if ( this_context->match_not_regex_str != NULL )
		(void) free(this_context->match_not_regex_str);
	if ( this_context->match_not_regex != NULL ) {
		regfree(this_context->match_not_regex);
		(void) free(this_context->match_not_regex);
	}
	free_tokens(this_context->action_tokens);
	(void) free(this_context);
	return;
}


/*
 * unlink a context (delte it from the linked list with all contexts)
 */
void
unlink_context(this_context)
	struct context	*this_context;
{
	if ( this_context->previous == NULL ) {
		all_contexts=this_context->next;
		if ( this_context->next == NULL )
			all_contexts_end=NULL;
		else
			this_context->next->previous=NULL;
	}
	else if ( this_context->next == NULL ) {
		all_contexts_end=this_context->previous;
		this_context->previous->next=NULL;
	}
	else {
		this_context->previous->next=this_context->next;
		this_context->next->previous=this_context->previous;
	}
	destroy_context(this_context);
	return;
}


/*
 * close one or more contexts
 */
void
close_context(context_def)
	char	*context_def;
{
	char		*src, *help_ptr;
	struct context	*context_ptr;

	src=context_def;
	while ( (all_contexts != NULL) && ((help_ptr=get_word(&src)) != NULL) ) {
		if ( (context_ptr=find_context(help_ptr)) != NULL )
			unlink_context(context_ptr);
		(void) free(help_ptr);
	}
	return;
}


/*
 * find a context by comparing the match_regex_str
 */
struct context *
find_context(compare_regex)
	char	*compare_regex;
{
	struct context	*context_ptr;

	for ( context_ptr=all_contexts; context_ptr != NULL;
		context_ptr=context_ptr->next )
		if ( context_ptr->match_regex_str != NULL )
			if ( !strcmp(context_ptr->match_regex_str, compare_regex) )
				return(context_ptr);
	return(NULL);
}


/*
 * add a line to a given context
 */
void
add_to_context(this_context, new_context_line)
	struct context		*this_context;
	struct context_line	*new_context_line;
{
	struct context_body	*new_context_body;

	if( this_context->action_type == ACTION_PIPE || this_context->action_type == ACTION_REPORT ){

	  /*
	   * init the new context body
	   */
	  if ( (new_context_body=(struct context_body *)
		malloc(sizeof(struct context_body))) == NULL ) {
		(void) fprintf(stderr, "out of memory updating context\n");
		(void) fprintf(stderr, "dropping line: %s\n", new_context_line->content);
		return;
	  }
	  new_context_body->this_line=new_context_line;
	  new_context_body->next=NULL;

	  /*
	   * link it into the context
	   */
	  if ( this_context->last == NULL ) {
		this_context->body=new_context_body;
		this_context->last=new_context_body;
	  }
	  else {
		this_context->last->next=new_context_body;
		this_context->last=new_context_body;
	  }

	  /*
	   * increment the linkt counter
	   */
	  new_context_line->link_counter++;

	}
	this_context->lines++;

	/*
	 * adjust a relative timeout (if used)
	 */
	if ( this_context->timeout_rel_offset != 0 ) {
		this_context->timeout_rel=(long) current_time +
			this_context->timeout_rel_offset;
		if ( this_context->timeout_rel < next_context_timeout )
			next_context_timeout=this_context->timeout_rel;
	}
	return;
}


/*
 * launch the default action of the given context
 */
void
do_context_action(this_context)
	struct context	*this_context;
{
	struct action_tokens	*dummy_action;;

	switch (this_context->action_type) {
	case ACTION_IGNORE:
		break;
	case ACTION_EXEC:
		expand_context_action_macros( this_context );
		do_exec(this_context->action_tokens);
		break;
	case ACTION_PIPE:
		if (this_context->action_tokens->this_word == NULL) {
			(void) fprintf(stderr, "pipe without command\n");
			return;
		}
		if (this_context->action_tokens->next != NULL ) {
			(void) fprintf(stderr, "pipe with multiple commands\n");
			return;
		}
		if ((dummy_action=(struct action_tokens *)
			malloc(sizeof(struct action_tokens))) == NULL ) {
			(void) fprintf(stderr, "out of memory preparing pipe cmd\n");
			return;
		}
		if ((dummy_action->this_word=(char *)
			malloc(strlen(this_context->match_regex_str)+1)) == NULL ) {
			(void) fprintf(stderr, "out of memory preparing pipe cmd\n");
			return;
		}
		(void) strcpy(dummy_action->this_word, this_context->match_regex_str);
		dummy_action->next=NULL;
		this_context->action_tokens->next=dummy_action;
		expand_context_action_macros( this_context );
		make_report(this_context->action_tokens, this_context->body);
		this_context->action_tokens->next=NULL;
		(void) free(dummy_action->this_word);
		(void) free(dummy_action);
		break;
	case ACTION_REPORT:
		expand_context_action_macros( this_context );
		make_report(this_context->action_tokens, this_context->body);
		break;
	case ACTION_ECHO:
		expand_context_action_macros( this_context );
		do_echo(this_context->action_tokens);
		break;
	case ACTION_SYSLOG:
		expand_context_action_macros( this_context );
		do_syslog(this_context->action_tokens);
		break;
	default:
		(void) fprintf(stderr,
			"internal error launching default action: unknown action\n");
		break;
	}

	return;
}


/*
 * check all context for timeouts (and remove timeouted context after
 * laumching the default_action
 */
void
check_context_timeout()
{
	struct context	*this_context, *next_context;

	next_context_timeout=LONG_MAX;
	this_context=all_contexts;
	while ( this_context != NULL ) {
		next_context=this_context->next;
		if ( (this_context->timeout_abs < (long) current_time) ||
			(this_context->timeout_rel < (long) current_time) ) {

		        if (!this_context->min_lines || this_context->lines >= this_context->min_lines )
			  do_context_action(this_context);
			unlink_context(this_context);
		}
		else {
			if ( this_context->timeout_abs < next_context_timeout )
				next_context_timeout=this_context->timeout_abs;
			if ( this_context->timeout_rel < next_context_timeout )
				next_context_timeout=this_context->timeout_rel;
		}
		this_context=next_context;
	}
	return;
}


/*
 * check all context for line limits (and remove affected contexts after
 * launching the default_action)
 */
void
check_context_linelimit()
{
	struct context	*this_context, *next_context;

	this_context=all_contexts;
	while ( this_context != NULL ) {
		next_context=this_context->next;
		if ( this_context->lines > this_context->max_lines ) {
		        if (!this_context->min_lines || this_context->lines >= this_context->min_lines )
			        do_context_action(this_context);
			unlink_context(this_context);
		}
		this_context=next_context;
	}
	return;
}


/*
 * expand any macro functions in a string:
 *   $lines -> replaced with count of lines in a context
 */
#define TEMPSTR_SZ 2048
void expand_context_action_macros( this_context )
struct context  *this_context;{
	char *pos;
	int cpos;
	char tempstr[TEMPSTR_SZ], *tp;
	char *new_string;
	char **cmd_string;
	struct action_tokens *act_token;
	int replacements = 0;

	/*
	 * Loop through all action tokens in the context
	 */
	for( act_token=this_context->action_tokens ; act_token != NULL ; act_token=act_token->next ){
	  cmd_string = &act_token->this_word;
	  for( pos=*cmd_string, tp=tempstr ; *pos ; pos++ ){
	    if( *pos == '$' && ( pos == *cmd_string || *(pos-1) != '\\' ) ){
              if( !strncmp(pos+1,"lines",5) ){
                tp += sprintf(tp,"%d",this_context->lines);
		pos += 5;
		replacements++;
              } else {
		*tp++ = *pos;
	      }
            } else {
                *tp++ = *pos;
            }
	    /* Avoid stack overflow in temp_str */
	    if( tp > (tempstr + TEMPSTR_SZ - 16) ){
	      fprintf(stderr,"action string too long in expand_context_action_macros");
	      return;
	    }
          }
	  *tp = '\0';
		  
	  if( replacements ){
	    /*
	     * malloc space for the new string, and replace the old on in the action list
	     */
	    if( (new_string = (char *)malloc(strlen(tempstr)+1)) == NULL){
	      (void) fprintf(stderr,"malloc failed in expand_context_action_macros\n");
	      return;
	    }
	    strcpy(new_string,tempstr);
	    free( *cmd_string );
	    *cmd_string = new_string;
	  }
	}

}


