/*
 * str_util.h
 *
 * header file: some small utility functions to handle strings...
 */

#if __STDC__

char	*skip_spaces(char *);
void	trim(char *);
char	*get_word(char **);

#else /* __STDC__ */

char	*skip_spaces();
void	trim();
char	*get_word();

#endif /* __STDC__ */
