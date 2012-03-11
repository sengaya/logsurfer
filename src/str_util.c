/*
 * str_util.c
 *
 * some small utility functions to handle strings...
 */


/* config header if available */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


/* system includes */

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

#if HAVE_MEMMOVE
/* everything fine */
#else
#if HAVE_BCOPY
#define memmove(dst,src,len)    bcopy(src,dst,len)
#endif
#endif


/* local includes */

#include "logsurfer.h"
#include "globals.h"
#include "regex.h"
#include "str_util.h"


/*
 * skip over all whitespaces at the begining of a string
 */
char *
skip_spaces(line_ptr)
	char	*line_ptr;
{
	if ( line_ptr == NULL )
		return NULL;
	while ( *line_ptr != '\0' && isspace(*line_ptr) )
		line_ptr++;
	return(line_ptr);
}


/*
 * trim a string (delete leading and pending spaces)
 */
void
trim(line)
	char	*line;
{
	char	*help_ptr;

	if ( line ==  NULL )
		return;
	if ( line[0] == '\0' )
		return;
	help_ptr=skip_spaces(line);
	if ( help_ptr != line )
		(void) memmove(line, help_ptr, strlen(help_ptr)+1);

	if ( line[0] == '\0' )
		return;
	help_ptr=&line[strlen(line)-1];
	while ( *help_ptr != '\0' && isspace(*help_ptr) )
		*help_ptr--='\0';
	return;
}


/*
 * get a word from a string (maybe quoted)
 *
 * return a pointer to a malloc()ed string or NULL in case of error;
 * in case of success the argument points to the first char behind the word
 */
char *
get_word(input_line)
	char	**input_line;
{
	char	*word_buffer;
	char	*src, *dst, *help_ptr;
	char	*result;
	int	space_left, add_space;
	int	var_num;

	if ( input_line == NULL )
		return(NULL);

	/* compute the maximum result string length */
	add_space=space_left=strlen(*input_line)+2;
	if ( logline != NULL )
		if ( strlen(logline) > add_space )
			add_space=strlen(logline);
	/* now do a quick scan on the string to find the max. number of $0's */
	for ( src=(*input_line); *src != '\0'; )
		if ( *src++ == '$' )
			space_left+=add_space;
	/* now space_left is the upper limit for the result length */
	if ( (word_buffer=(char *)malloc(space_left)) == NULL ) {
		(void) fprintf(stderr, "out of memory reserving space for a word in: %s\n",
			*input_line);
		return(NULL);
	}

	dst=word_buffer;
	if ( *(src=skip_spaces(*input_line)) == '\0' ) {
		(void) free(word_buffer);
		return(NULL);
	}
	/* different quoting modes */
	switch ( *src ) {
	case '\"':
		src++;
		while ( (*src != '\0') && (*src != '\"') && (space_left > 0) ) {
			if ( (*src == '\\') && (*(src+1) != '\0') ) {
				src++;
				*dst++=*src++;
				space_left--;
			}
			else if ( (*src == '$') && (*(src+1) >= '0') && (*(src+1) <= '9') ) {
				if ( (var_num=(*++src)-48) == 0 ) {
					help_ptr=logline;
					while ( (*help_ptr != '\0') && (space_left > 0) ) {
						*dst++=*help_ptr++;
						space_left--;
					}
				}
				else if ( var_num > regex_submatches_num+1 ) {
					*dst++='$';
					if ( --space_left > 0 ) {
						*dst++=*src;
						space_left--;
					}
				}
				else {
					help_ptr=&logline[regex_submatches.start[--var_num]];
					while ( (help_ptr != 
						&logline[regex_submatches.end[var_num]]) &&
						(space_left > 0) ) {
						*dst++=*help_ptr++;
						space_left--;
					}
				}
				src++;
			}
			else {
				*dst++=*src++;
				space_left--;
			}
		}
		if ( (space_left == 0) || (*src != '\"') ) {
			(void) free(word_buffer);
			return(NULL);
		}
		src++;
		break;
	case '\'':
		src++;
		while ( (*src != '\0') && (*src != '\'') && (space_left > 0) ) {
			*dst++=*src++;
			space_left--;
		}
		if ( (space_left == 0) || (*src != '\'') ) {
			(void) free(word_buffer);
			return(NULL);
		}
		src++;
		break;
	default:
		while ( (*src != '\0') && (!isspace(*src)) && (space_left > 0) ) {
			if ( (*src == '\\') && (*(src+1) != '\0') ) {
				src++;
				*dst++=*src++;
				space_left--;
			}
			else if ( (*src == '$') && (*(src+1) >= '0') && (*(src+1) <= '9') ) {
				if ( (var_num=(*++src)-48) == 0 ) {
					help_ptr=logline;
					while ( (*help_ptr != '\0') && (space_left > 0) ) {
						*dst++=*help_ptr++;
						space_left--;
					}
				}
				else if ( var_num > regex_submatches_num+1 ) {
					*dst++='$';
					if ( --space_left > 0 ) {
						*dst++=*src;
						space_left--;
					}
				}
				else {
					help_ptr=&logline[regex_submatches.start[--var_num]];
					while ( (help_ptr != 
						&logline[regex_submatches.end[var_num]]) &&
						(space_left > 0) ) {
						*dst++=*help_ptr++;
						space_left--;
					}
				}
				src++;
			}
			else {
				*dst++=*src++;
				space_left--;
			}
		}
		if ( space_left == 0 ) {
			(void) free(word_buffer);
			return(NULL);
		}
		break;
	}
	*dst='\0';
	if ( (result=(char *)malloc(strlen(word_buffer)+1)) == NULL ) {
		(void) free(word_buffer);
		return NULL;
	}
	(void) strcpy(result, word_buffer);
	(void) free(word_buffer);
	*input_line=src;
	return(result);
}



