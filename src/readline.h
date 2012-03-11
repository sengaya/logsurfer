/*
 * readline.h
 *
 * header file: routine to read lines from a textfile
 */

#if __STDC__

int	expand_buf(char **, int *, int);
char	*readline(FILE *, char **, int *, int *);

#else /* __STDC__ */

int	expand_buf();
char	*readline();

#endif /* __STDC__ */
