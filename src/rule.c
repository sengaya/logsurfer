/*
 * rule.c
 *
 * routines to handle rules (create, delete, ...)
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

#if HAVE_LIMITS_H
#include <limits.h>
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
#include "context.h"
#include "exec.h"
#include "rule.h"


/*
 * malloc() and initialize a new rule
 */
struct rule *
create_rule()
{
	struct rule	*result;

	if ( (result=(struct rule *)malloc(sizeof(struct rule))) == NULL )
		return NULL;

	result->match_regex=NULL;
	result->match_regex_str=NULL;
	result->match_not_regex=NULL;
	result->match_not_regex_str=NULL;
	result->stop_regex=NULL;
	result->stop_regex_str=NULL;
	result->stop_not_regex=NULL;
	result->stop_not_regex_str=NULL;
	result->timeout=LONG_MAX;
	result->do_continue=0;
	result->action_type=ACTION_UNKNOWN;
	result->action_body=NULL;
	result->next=NULL;
	result->previous=NULL;

	return(result);
}


/*
 * destroy (free) a rule
 */
void
destroy_rule(rule_ptr)
	struct rule	*rule_ptr;
{
	if ( rule_ptr == NULL )
		return;

	if ( rule_ptr->match_regex != NULL ) {
		regfree(rule_ptr->match_regex);
		(void) free(rule_ptr->match_regex);
	}
	if ( rule_ptr->match_regex_str != NULL )
		(void) free(rule_ptr->match_regex_str);
	if ( rule_ptr->match_not_regex != NULL ) {
		regfree(rule_ptr->match_not_regex);
		(void) free(rule_ptr->match_not_regex);
	}
	if ( rule_ptr->match_not_regex_str != NULL )
		(void) free(rule_ptr->match_not_regex_str);

	if ( rule_ptr->stop_regex != NULL ) {
		regfree(rule_ptr->stop_regex);
		(void) free(rule_ptr->stop_regex);
	}
	if ( rule_ptr->stop_regex_str != NULL )
		(void) free(rule_ptr->stop_regex_str);
	if ( rule_ptr->stop_not_regex != NULL ) {
		regfree(rule_ptr->stop_not_regex);
		(void) free(rule_ptr->stop_not_regex);
	}
	if ( rule_ptr->stop_not_regex_str != NULL )
		(void) free(rule_ptr->stop_not_regex_str);

	if ( rule_ptr->action_body != NULL )
		(void) free(rule_ptr->action_body);
	if ( rule_ptr->previous == NULL ) {
		if ( all_rules == rule_ptr )
			all_rules=rule_ptr->next;
	}
	else
		rule_ptr->previous->next=rule_ptr->next;
	if ( rule_ptr->next != NULL )
		rule_ptr->next->previous=rule_ptr->previous;
	if ( all_rules_end == rule_ptr )
		all_rules_end=rule_ptr->previous;
	(void) free(rule_ptr);
	return;
}


/*
 * unlink a rule (delete it from the linked list of all rules)
 */
void
unlink_rule(this_rule)
	struct rule	*this_rule;
{
	if ( this_rule->previous == NULL ) {
		all_rules=this_rule->next;
		if ( this_rule->next == NULL )
			all_rules_end=NULL;
		else
			this_rule->next->previous=all_rules;
	}
	else if ( this_rule->next == NULL ) {
		all_rules_end=this_rule->previous;
		this_rule->previous->next=NULL;
	}
	else {
		this_rule->previous->next=this_rule->next;
		this_rule->next->previous=this_rule->previous;
	}
	destroy_rule(this_rule);
	return;
}


/*
 * parse a rule string and return a pointer to a new rule
 */
struct rule *
parse_rule(input_line)
	char	*input_line;
{
	struct rule	*new_rule;			/* the new rule to add	*/
	char		*help_ptr;

	if ( (new_rule=create_rule()) == NULL ) {
		(void) fprintf(stderr, "out of memory for rules\n");
		return(NULL);
	}

	/* get the match_regex */
	help_ptr=input_line;
	if ( (new_rule->match_regex_str=get_word(&help_ptr)) == NULL ) {
		(void) fprintf(stderr, "error in match_regex of rule: %s\n", input_line);
		destroy_rule(new_rule);
		return(NULL);
	}
	if ( (new_rule->match_regex=(struct re_pattern_buffer *)
		malloc(sizeof(struct re_pattern_buffer))) == NULL ) {
		(void) fprintf(stderr, "out of memory adding rule: %s\n", input_line);
		destroy_rule(new_rule);
		return(NULL);
	}
	/* use fastmap if enough memeory available */
	new_rule->match_regex->fastmap=(char *)malloc(256);
	new_rule->match_regex->translate=(char *) 0;
	new_rule->match_regex->buffer=NULL;
	new_rule->match_regex->allocated=0;
	if ( re_compile_pattern(new_rule->match_regex_str, strlen(new_rule->match_regex_str),
		new_rule->match_regex) != 0 ) {
		(void) fprintf(stderr, "error in match_regex of rule: %s\n", input_line);
		destroy_rule(new_rule);
		return(NULL);
	}
	new_rule->match_regex->regs_allocated=REGS_FIXED;

	/* get the option match_not_regex (the expresion that should *not* match) */
	help_ptr=skip_spaces(help_ptr);
	if ( *help_ptr != '-' ) {
		if ( (new_rule->match_not_regex_str=get_word(&help_ptr)) == NULL ) {
			(void) fprintf(stderr, "error in match_not_regex of rule: %s\n", input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		if ( (new_rule->match_not_regex=(struct re_pattern_buffer *)
			malloc(sizeof(struct re_pattern_buffer))) == NULL ) {
			(void) fprintf(stderr, "out of memory adding rule: %s\n", input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		/* use fastmap if enough memeory available */
		new_rule->match_not_regex->fastmap=(char *)malloc(256);
		new_rule->match_not_regex->translate=(char *) 0;
		new_rule->match_not_regex->buffer=NULL;
		new_rule->match_not_regex->allocated=0;
		if ( re_compile_pattern(new_rule->match_not_regex_str,
			strlen(new_rule->match_not_regex_str),
			new_rule->match_not_regex) != 0 ) {
			(void) fprintf(stderr, "error in match_not_regex of rule: %s\n", input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		new_rule->match_not_regex->regs_allocated=REGS_FIXED;
	}
	else
		help_ptr++;

	/* get the optional stop_regex */
	help_ptr=skip_spaces(help_ptr);
	if ( *help_ptr != '-' ) {
		if ( (new_rule->stop_regex_str=get_word(&help_ptr)) == NULL ) {
			(void) fprintf(stderr, "error in stop_regex of rule: %s\n", input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		if ( (new_rule->stop_regex=(struct re_pattern_buffer *)
			malloc(sizeof(struct re_pattern_buffer))) == NULL ) {
			(void) fprintf(stderr, "out of memory adding rule: %s\n", input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		/* use fastmap if enough memeory available */
		new_rule->stop_regex->fastmap=(char *)malloc(256);
		new_rule->stop_regex->translate=(char *) 0;
		new_rule->stop_regex->buffer=NULL;
		new_rule->stop_regex->allocated=0;
		if ( re_compile_pattern(new_rule->stop_regex_str, strlen(new_rule->stop_regex_str),
			new_rule->stop_regex) != 0 ) {
			(void) fprintf(stderr, "error in stop_regex of rule: %s\n", input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		new_rule->stop_regex->regs_allocated=REGS_FIXED;
	}
	else
		help_ptr++;

	/* get the option stop_not_regex (the expresion that should *not* match) */
	help_ptr=skip_spaces(help_ptr);
	if ( *help_ptr != '-' ) {
		if ( (new_rule->stop_not_regex_str=get_word(&help_ptr)) == NULL ) {
			(void) fprintf(stderr, "error in stop_not_regex of rule: %s\n", input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		if ( (new_rule->stop_not_regex=(struct re_pattern_buffer *)
			malloc(sizeof(struct re_pattern_buffer))) == NULL ) {
			(void) fprintf(stderr, "out of memory adding rule: %s\n", input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		/* use fastmap if enough memeory available */
		new_rule->stop_not_regex->fastmap=(char *)malloc(256);
		new_rule->stop_not_regex->translate=(char *) 0;
		new_rule->stop_not_regex->buffer=NULL;
		new_rule->stop_not_regex->allocated=0;
		if ( re_compile_pattern(new_rule->stop_not_regex_str,
			strlen(new_rule->stop_not_regex_str),
			new_rule->stop_not_regex) != 0 ) {
			(void) fprintf(stderr, "error in stop_not_regex of rule: %s\n", input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		new_rule->stop_not_regex->regs_allocated=REGS_FIXED;
	}
	else
		help_ptr++;

	/* get the optinal timeout value */
	help_ptr=skip_spaces(help_ptr);
	new_rule->timeout=atol(help_ptr);
	while ( *help_ptr != '\0' && !isspace(*help_ptr) )
		help_ptr++;
	if ( new_rule->timeout == 0 )
		new_rule->timeout=LONG_MAX;
	else {
		new_rule->timeout+=(long) current_time;
		if ( new_rule->timeout < next_rule_timeout )
			next_rule_timeout=new_rule->timeout;
	}

	/* check for the contiue keyword */
	help_ptr=skip_spaces(help_ptr);
	if (!strncasecmp(help_ptr, "continue", 8)) {
		new_rule->do_continue=1;
		help_ptr=skip_spaces(help_ptr+8);
	}
	else
		new_rule->do_continue=0;

	/* now parse the action type */
	if (!strncasecmp(help_ptr, "ignore", 6)) {
		new_rule->action_type=ACTION_IGNORE;
	}
	else if (!strncasecmp(help_ptr, "exec", 4)) {
		new_rule->action_type=ACTION_EXEC;
		help_ptr=skip_spaces(help_ptr+4);
		if ( *help_ptr == '\0' ) {
			(void) fprintf(stderr, "exec without cmd in rule: %s\n",
				input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		if ( (new_rule->action_body=(char *)malloc(strlen(help_ptr)+1)) == NULL ) {
			(void) fprintf(stderr, "out of memory parsing rule\n");
			destroy_rule(new_rule);
			return(NULL);
		}
		(void) strcpy(new_rule->action_body, help_ptr);
	}
	else if (!strncasecmp(help_ptr, "pipe", 4)) {
		new_rule->action_type=ACTION_PIPE;
		help_ptr=skip_spaces(help_ptr+4);
		if ( *help_ptr == '\0' ) {
			(void) fprintf(stderr, "pipe without cmd in rule: %s\n",
				input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		if ( (new_rule->action_body=(char *)malloc(strlen(help_ptr)+1)) == NULL ) {
			(void) fprintf(stderr, "out of memory parsing rule\n");
			destroy_rule(new_rule);
			return(NULL);
		}
		(void) strcpy(new_rule->action_body, help_ptr);
	}
	else if (!strncasecmp(help_ptr, "open", 4)) {
		new_rule->action_type=ACTION_OPEN;
		help_ptr=skip_spaces(help_ptr+4);
		if ( *help_ptr == '\0' ) {
			(void) fprintf(stderr, "open without context in rule: %s\n",
				input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		if ( (new_rule->action_body=(char *)malloc(strlen(help_ptr)+1)) == NULL ) {
			(void) fprintf(stderr, "out of memory parsing rule\n");
			destroy_rule(new_rule);
			return(NULL);
		}
		(void) strcpy(new_rule->action_body, help_ptr);
	}
	else if (!strncasecmp(help_ptr, "delete", 6)) {
		new_rule->action_type=ACTION_DELETE;
		help_ptr=skip_spaces(help_ptr+6);
		if ( *help_ptr == '\0' ) {
			(void) fprintf(stderr, "delete without context_regex in rule: %s\n",
				input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		if ( (new_rule->action_body=(char *)malloc(strlen(help_ptr)+1)) == NULL ) {
			(void) fprintf(stderr, "out of memory parsing rule\n");
			destroy_rule(new_rule);
			return(NULL);
		}
		(void) strcpy(new_rule->action_body, help_ptr);
	}
	else if (!strncasecmp(help_ptr, "report", 6)) {
		new_rule->action_type=ACTION_REPORT;
		help_ptr=skip_spaces(help_ptr+6);
		if ( *help_ptr == '\0' ) {
			(void) fprintf(stderr, "report without cmd/context_regex in rule: %s\n",
				input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		if ( (new_rule->action_body=(char *)malloc(strlen(help_ptr)+1)) == NULL ) {
			(void) fprintf(stderr, "out of memory parsing rule\n");
			destroy_rule(new_rule);
			return(NULL);
		}
		(void) strcpy(new_rule->action_body, help_ptr);
	}
	else if (!strncasecmp(help_ptr, "rule", 4)) {
		new_rule->action_type=ACTION_RULE;
		help_ptr=skip_spaces(help_ptr+4);
		if ( *help_ptr == '\0' ) {
			(void) fprintf(stderr, "rule adding without rule in rule: %s\n",
				input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		if ( (new_rule->action_body=(char *)malloc(strlen(help_ptr)+1)) == NULL ) {
			(void) fprintf(stderr, "out of memory parsing rule\n");
			destroy_rule(new_rule);
			return(NULL);
		}
		(void) strcpy(new_rule->action_body, help_ptr);
	}
	else if (!strncasecmp(help_ptr, "echo", 4)) {
		new_rule->action_type=ACTION_ECHO;
		help_ptr=skip_spaces(help_ptr+4);
		if ( *help_ptr == '\0' ) {
			(void) fprintf(stderr, "echo without string in rule: %s\n",
				input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		if ( (new_rule->action_body=(char *)malloc(strlen(help_ptr)+1)) == NULL ) {
			(void) fprintf(stderr, "out of memory parsing rule\n");
			destroy_rule(new_rule);
			return(NULL);
		}
		(void) strcpy(new_rule->action_body, help_ptr);
	}
	else if (!strncasecmp(help_ptr, "syslog", 6)) {
		new_rule->action_type=ACTION_SYSLOG;
		help_ptr=skip_spaces(help_ptr+6);
		if ( *help_ptr == '\0' ) {
			(void) fprintf(stderr, "syslog without arguments in rule: %s\n",
				input_line);
			destroy_rule(new_rule);
			return(NULL);
		}
		if ( (new_rule->action_body=(char *)malloc(strlen(help_ptr)+1)) == NULL ) {
			(void) fprintf(stderr, "out of memory parsing rule\n");
			destroy_rule(new_rule);
			return(NULL);
		}
		(void) strcpy(new_rule->action_body, help_ptr);
	}
	else {
		(void) fprintf(stderr, "unknown action in rule: %s\n", input_line);
		destroy_rule(new_rule);
		return(NULL);
	}

	new_rule->next=NULL;
	new_rule->previous=NULL;
	return(new_rule);
}


/*
 * dynamically add a rule
 */
void
dynamic_add_rule(this_rule)
	struct rule	*this_rule;
{
	char		*src, *add_where;
	struct rule	*new_rule;

	src=this_rule->action_body;
	if ( (add_where=get_word(&src)) == NULL ) {
		(void) fprintf(stderr, "don't know where to add rule: %s\n",
			this_rule->action_body);
		return;
	}
	
	if ( (new_rule=parse_rule(src)) == NULL ) {
		(void) fprintf(stderr, "can't parse rule: %s\n", src);
		return;
	}

	/* check where to add the new rule */
	if ( !strcmp(add_where, "before") )
		add_rule_before(new_rule, this_rule);
	else if ( !strcmp(add_where, "behind") )
		add_rule_behind(new_rule, this_rule);
	else if ( !strcmp(add_where, "top") )
		add_rule_before(new_rule, all_rules);
	else if ( !strcmp(add_where, "bottom") )
		add_rule_behind(new_rule, all_rules_end);
	else
		(void) fprintf(stderr, "don't know where to add rule: %s\n",
			this_rule->action_body);

	(void) free(add_where);

	return;
}


/*
 * add a new rule behind another rule
 */
void
add_rule_behind(new_rule, old_rule)
	struct rule	*new_rule;
	struct rule	*old_rule;
{
	if ( old_rule->next == NULL ) {
		/* there is no next rule so we are at the end */
		old_rule->next=new_rule;
		new_rule->previous=old_rule;
		all_rules_end=new_rule;
	}
	else {
		new_rule->next=old_rule->next;
		new_rule->previous=old_rule;
		old_rule->next=new_rule;
		if ( new_rule->next != NULL )
			new_rule->next->previous=new_rule;
	}
	return;
}


/*
 * add a rule in front of another rule
 */
void
add_rule_before(new_rule, old_rule)
	struct rule	*new_rule;
	struct rule	*old_rule;
{
	if ( old_rule->previous == NULL ) {
		/* there is no previous so we are at the top */
		new_rule->previous=NULL;
		new_rule->next=old_rule;
		old_rule->previous=new_rule;
		all_rules=new_rule;
		return;
	}
	add_rule_behind(new_rule, old_rule->previous);
	return;
}


/*
 * process the given rule (we found a match)
 */
void
process_rule(this_rule)
	struct rule	*this_rule;
{
	struct action_tokens	*help_ptr;

	switch ( this_rule->action_type ) {
	case ACTION_IGNORE:
		break;
	case ACTION_EXEC:
		if ((help_ptr=collect_tokens(this_rule->action_body)) == NULL) {
			(void) fprintf(stderr, "out of memory generating report: %s\n",
				this_rule->action_body);
			return;
		}
		do_exec(help_ptr);
		free_tokens(help_ptr);
		break;
	case ACTION_PIPE:
		if ((help_ptr=collect_tokens(this_rule->action_body)) == NULL) {
			(void) fprintf(stderr, "out of memory generating report: %s\n",
				this_rule->action_body);
			return;
		}
		do_pipe_line(help_ptr);
		free_tokens(help_ptr);
		break;
	case ACTION_OPEN:
		open_context(this_rule->action_body);
		break;
	case ACTION_DELETE:
		close_context(this_rule->action_body);
		break;
	case ACTION_REPORT:
		if ((help_ptr=collect_tokens(this_rule->action_body)) == NULL) {
			(void) fprintf(stderr, "out of memory generating report: %s\n",
				this_rule->action_body);
			return;
		}
		make_report(help_ptr,NULL);
		free_tokens(help_ptr);
		break;
	case ACTION_RULE:
		dynamic_add_rule(this_rule);
		break;
	case ACTION_ECHO:
		if ((help_ptr=collect_tokens(this_rule->action_body)) == NULL) {
			(void) fprintf(stderr, "out of memory generating echo: %s\n",
				this_rule->action_body);
			return;
		}
		do_echo(help_ptr);
		free_tokens(help_ptr);
		break;
	case ACTION_SYSLOG:
		if ((help_ptr=collect_tokens(this_rule->action_body)) == NULL) {
			(void) fprintf(stderr, "out of memory generating syslog: %s\n",
				this_rule->action_body);
			return;
		}
		do_syslog(help_ptr);
		free_tokens(help_ptr);
		break;
	}
	return;
}


/*
 * check all exisitng rules for timeouts (and delete the rules with timeouts)
 */
void
check_rule_timeout()
{
	struct rule	*rule_ptr, *next_rule;

	next_rule_timeout=LONG_MAX;
	rule_ptr=all_rules;
	while ( rule_ptr != NULL ) {
		next_rule=rule_ptr->next;
		if ( rule_ptr->timeout < (long) current_time )
			unlink_rule(rule_ptr);
		else if ( rule_ptr->timeout < next_rule_timeout )
			next_rule_timeout=rule_ptr->timeout;
		rule_ptr=next_rule;
	}
	return;
}


