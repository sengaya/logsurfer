/*
 * readcf.h
 *
 * header file: routines to read and parse the configuration file
 */

#if __STDC__

void	process_cfline(char *, long);
int	read_config(char *);

#else /* __STDC__ */

void	process_cfline();
int	read_config();

#endif /* __STDC__ */

