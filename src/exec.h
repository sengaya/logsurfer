/*
 * exec.c
 *
 * header file: routines to invoke externale programs
 */


#if __STDC__

int			prepare_exec(char *);
void			do_exec(struct action_tokens *);
void			do_pipe_line(struct action_tokens *);
void			do_echo(struct action_tokens *);
void			do_syslog(struct action_tokens *);
int			parse_syslog_faclevel(char *, int *, int *);

void			free_tokens(struct action_tokens *);
struct action_tokens	*collect_tokens(char *);

#ifdef SENDMAIL_FLUSH
void			flush_queue();
#endif

#else /* __STDC__ */

int			prepare_exec();
void			do_exec();
void			do_pipe_line();
void			do_echo();
void			do_syslog();
int			parse_syslog_faclevel();

void			free_tokens();
struct action_tokens	*collect_tokens();

#ifdef SENDMAIL_FLUSH
void			flush_queue();
#endif

#endif /* __STDC__ */
