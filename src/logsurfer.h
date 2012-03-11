/*
 * logsurfer.h
 *
 * definition of types and some defines
 *
 */

#ifndef LOGSURFER_H
#define LOGSURFER_H

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

#ifndef CONFFILE
#define CONFFILE "/usr/local/etc/logsurfer.conf"
#endif

#ifndef DUMPFILE
#define DUMPFILE "/var/tmp/logsurfer.dump"
#endif


#ifdef SENDMAIL_FLUSH
#define FLUSH_DELAY	30
#endif

#define ACTION_UNKNOWN	0
#define ACTION_IGNORE	1
#define ACTION_EXEC	2
#define ACTION_PIPE	3
#define ACTION_OPEN	4
#define ACTION_DELETE	5
#define ACTION_REPORT	6
#define ACTION_RULE	7
#define ACTION_ECHO     8
#define ACTION_SYSLOG   9


struct context_line {
	long			linenumber;	/* the linenumber		*/
	long			timestamp;	/* timestamp (in seconds)	*/
	char			*content;	/* the logline			*/
	long			link_counter;	/* number of links to this line	*/
};

struct context_body {
	struct context_line	*this_line;	/* one context line		*/
	struct context_body	*next;		/* the next context line	*/
};

struct action_tokens {
	char			*this_word;	/* one token from the action	*/
	struct action_tokens	*next;		/* next token			*/
};

struct context {
	struct re_pattern_buffer *match_regex;	/* regular expression to match	*/
	char			*match_regex_str;/* the ascii string of regex	*/
	struct re_pattern_buffer *match_not_regex;
	char			*match_not_regex_str;
	long			max_lines;	/* maximum number of bodylines	*/
	long			min_lines;	/* minimum number of bodylines	*/
	long			timeout_abs;	/* absolut timeout		*/
	long			timeout_rel;	/* relativ timeout		*/
	long			timeout_rel_offset;	/* from the config line	*/
	int			action_type;	/* type of default action	*/
	struct action_tokens	*action_tokens;	/* content(s) of the action	*/

	struct context_body	*body;		/* content of the context	*/
	long			lines;		/* number of current lines	*/
	struct context_body	*last;		/* pointer to last bidyline	*/

	struct context		*next;		/* next context			*/
	struct context		*previous;	/* previous context		*/
};

struct rule {
	struct re_pattern_buffer *match_regex;	/* regular expression to match	*/
	char			*match_regex_str;	/* for debugging	*/
	struct re_pattern_buffer *match_not_regex;
	char			*match_not_regex_str;
	struct re_pattern_buffer *stop_regex;	/* delete rule			*/
	char			*stop_regex_str;	/* for debugging	*/
	struct re_pattern_buffer *stop_not_regex;
	char			*stop_not_regex_str;
	long			timeout;	/* timeout for this rule	*/
	int			do_continue;	/* continue flag		*/

	int			action_type;	/* which action follows		*/
	char			*action_body;	/* body (if any)		*/

	struct rule		*next;		/* pointer to next rule		*/
	struct rule		*previous;	/* pointer to previous rule	*/
};


#endif		/* LOGSURFER_H */

