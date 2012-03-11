/*
 * exit.h
 *
 * header file: routines to exit the logsurfer, dump the database and to cleanup
 */

#if __STDC__

void	dump_data();
void	real_dump_data();
void	cleanup_memory();
void	logsurfer_exit(int);

#else /* __STDC__ */

void	dump_data();
void	real_dump_data();
void	cleanup_memory();
void	logsurfer_exit();

#endif /* __STDC__ */
