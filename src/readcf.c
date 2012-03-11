/*
 * readcf.c
 *
 * routines to read and parse the configuration file
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


/* local includes */

#include "logsurfer.h"
#include "globals.h"
#include "str_util.h"
#include "rule.h"
#include "readline.h"
#include "readcf.h"


/*
 * process one line from the configuration file
 */
void
process_cfline(cfline, line_number)
	char	*cfline;
	long	line_number;
{
	struct rule	*new_rule;

	if ( (new_rule=parse_rule(cfline)) == NULL ) {
		(void) fprintf(stderr, "config error arround line %ld: %s\n",
			line_number, cfline);
		exit(4);
	}
	if ( all_rules == NULL ) {
		all_rules=new_rule;
		all_rules_end=new_rule;
	}
	else {
		all_rules_end->next=new_rule;
		new_rule->previous=all_rules_end;
		all_rules_end=new_rule;
	}
	return;
}


/*
 * read the configuration file
 */
int
read_config(filename)
	char	*filename;
{
	FILE	*infile;		/* filehandel for configfile	*/
	char	*buffer;		/* buffer for reading		*/
	int	buf_size, buf_pos;	/* used for buffer description	*/
	char	*cfline;		/* one configuration line	*/
	int	cfline_size;		/* size of cfline buffer	*/
	char	*input_line;		/* a new line from the cf file	*/
	long	line_number;		/* linenumber within configfile */

	if ( (infile=fopen(filename, "r")) == NULL ) {
		(void) fprintf(stderr, "error opening configfile %s\n",
			filename);
		return(1);
	}
	line_number=0;

	/* we start with input lines not longer than 1024 bytes */
	buf_size=1024;
	buf_pos=0;
	if ( (buffer=(char *)malloc(buf_size)) == NULL ) {
		(void) fprintf(stderr, "memory problems reading configfile\n");
		return(1);
	}
	buffer[0] = '\0';
	/* dito for one complete configline */
	cfline_size=1024;
	if ( (cfline=(char *)malloc(cfline_size)) == NULL ) {
		(void) fprintf(stderr, "memory problems reading configfile\n");
		return(1);
	}
	cfline[0] = '\0';

	while ( ((input_line=readline(infile, &buffer, &buf_size, &buf_pos)) != NULL) ||
		(buf_pos != 0) ) {
		/* did we read in an incomplete line at the end of file? */
		if ( input_line == NULL ) {
			/* so there must be some unprocessed stuff in the buffer */
			if ( (input_line=(char *) malloc(strlen(buffer)+1)) == NULL ) {
				(void) fprintf(stderr, "memory problems reading configfile\n");
				return(1);
			}
			(void) strcpy(input_line, buffer);
			buf_pos=0;
		}
		/* we got a new line from the config-file */
		line_number++;
		if ( (input_line[0] == '\0' || input_line[0] == '#'
			|| !isspace(input_line[0])) && cfline[0] != '\0' ) {
			process_cfline(cfline, line_number);
			cfline[0]='\0';
		}
		if ( input_line[0] == '\0' || input_line[0] == '#' ) {
			(void) free(input_line);
			continue;
		}
		if ( isspace(input_line[0]) ) {
			trim(input_line);
			/* add a space and the input_line to the cfline */
			if ( expand_buf(&cfline, &cfline_size,
				strlen(cfline)+1+strlen(input_line)+1) != 0 ) {
				(void) fprintf(stderr, "memory problems reading configfile");
				return(1);
			}
			(void) strcat(cfline, " ");
			(void) strcat(cfline, input_line);
			(void) free(input_line);
			continue;
		}
		if ( cfline[0] != '\0' ) {
			process_cfline(cfline, line_number);
			cfline[0]='\0';
		}
		/* copy the input_line to the cfline */
		if ( expand_buf(&cfline, &cfline_size, strlen(input_line)+1) != 0 ) {
			(void) fprintf(stderr, "memory problems reading configfile");
			return(1);
		}
		if ( expand_buf(&cfline, &cfline_size, strlen(input_line)+1) != 0 ) {
			(void) fprintf(stderr, "memory problems reading configfile");
			return(1);
		}
		(void) strcpy(cfline, input_line);
		(void) free(input_line);
	}
	if ( cfline[0] != '\0' )
		process_cfline(cfline, line_number);

	(void) fclose(infile);
	return(0);
}

