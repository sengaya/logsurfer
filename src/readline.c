/*
 * readline.c
 *
 * routine to read lines from a textfile
 */


/* config header if available */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

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

#if HAVE_MALLOC_H
#include <malloc.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_MEMMOVE
/* everything fine */
#else
#if HAVE_BCOPY
#define memmove(dst,src,len)    bcopy(src,dst,len)
#endif
#endif


/* local includes */

#include "readline.h"


/*
 * expand a buffer if neccessary
 */
int
expand_buf(buffer, buffer_size, needed)
	char    **buffer;
	int	*buffer_size;
	int	needed;
{
	while ( *buffer_size < needed ) {
		if ( (*buffer=(char *)realloc(*buffer, (*buffer_size)*2)) == NULL )
			return(1);
		*buffer_size=(*buffer_size)*2;
	}
	return(0);
}


/*
 * read a line from a textfile
 * - A line ends with a \n which is stripped from the result
 * - Null-bytes are replaced by underscores '_'
 * - If the buffer is not large enough for the line we use realloc() to double it
 *
 * return values:
 *  NULL    = error during read() (EOF?)
 *  char *  = a malloced string with the line we got from the file
 */
char *
readline(infile, buffer, buffer_size, buffer_pos)
	FILE	*infile;
	char	**buffer;
	int	*buffer_size;
	int	*buffer_pos;
{
	int	byte_count=0;
	char	*help_ptr;
	char	*result;
	char	*new_buffer;

#ifdef DEBUG
	(void) printf("readline() called. buffer_pos is %d\n", *buffer_pos);
#endif
	/* if we still have old data than first check if we have another
	   line in the buffer waiting */
	if ( (**buffer != '\0') && ((help_ptr=strchr(*buffer, '\n')) != NULL) ) {
#ifdef DEBUG
		(void) printf("still in buffer: %s\n", *buffer);
#endif
		*help_ptr++='\0';
		if ( (result=(char *) malloc(strlen(*buffer)+1)) == NULL ) {
			(void) fprintf(stderr, "out of memory reading input!\n");
			/* what should we do here??? */
			/* for now we return the EOF condition... */
			return(NULL);
		}
		(void) strcpy(result, *buffer);
		(void) memmove(*buffer, help_ptr, strlen(help_ptr)+1);
		*buffer_pos-=strlen(result)+1;

#ifdef DEBUG
		(void) printf("got line from old buffer: %s\n", result);
#endif
		return(result);
	}

	/* try to read some bytes from the file */
	if ((byte_count=read(fileno(infile), &(*buffer)[*buffer_pos],
		*buffer_size-*buffer_pos-1)) == 0 ) {
		return(NULL);
	}
	*buffer_pos+=byte_count;
	(*buffer)[*buffer_pos]='\0';

	/* replace null-bytes by underscores */
	while ( strlen(*buffer) != *buffer_pos ) {
		help_ptr=strchr(*buffer, '\0');
		*help_ptr='_';
	}

	/* do we have a complete line (terminated by a \n)? */
	if ( (help_ptr=strchr(*buffer, '\n')) != NULL ) {
		*help_ptr++='\0';
		if ( (result=(char *) malloc(strlen(*buffer)+1)) == NULL ) {
			(void) fprintf(stderr, "out of memory reading input!\n");
			/* what should we do here??? */
			/* for now we return the EOF condition... */
			return(NULL);
		}
		(void) strcpy(result, *buffer);
		(void) memmove(*buffer, help_ptr, strlen(help_ptr)+1);
		*buffer_pos-=strlen(result)+1;

#ifdef DEBUG
		(void) printf("got line from new buffer: %s\n", result);
#endif
		return(result);
	}

	/* we got some bytes, but not a complete line */
	/* increase the buffer space if neccessary and read more bytes */
	if ( *buffer_pos == (*buffer_size)-1 ) {
		/* buffer is full -> increase the buffer size */
		if ( (new_buffer=(char *)realloc(*buffer, (*buffer_size)*2)) == NULL ) {
			(void) fprintf(stderr, "out of memory reading input!\n");
			/* what should we do here??? */
			/* for now we return the EOF condition... */
			return(NULL);
		}
		*buffer=new_buffer;
		*buffer_size=(*buffer_size)*2;
	}
	return(readline(infile, buffer, buffer_size, buffer_pos));
}


