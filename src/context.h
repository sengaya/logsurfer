/*
 * context.h
 *
 * header file: routines to handle contexts (create, delete, ...)
 */


#if __STDC__

struct context	*create_context();
void		open_context(char *);

void		destroy_context_line(struct context_line *);
void		destroy_body(struct context_body *);
void		destroy_context(struct context *);
void		unlink_context(struct context *);
void		close_context(char *);

struct context	*find_context(char *);
void		add_to_context(struct context *, struct context_line *);

void		do_context_action(struct context *);

void		check_context_timeout();

void		expand_context_action_macros(struct context *);

#else /* __STDC__ */

struct context	*create_context();
void		open_context();

void		destroy_context_line();
void		destroy_body();
void		destroy_context();
void		unlink_context();
void		close_context();

struct context	*find_context();
void		add_to_context();

void		do_context_action();

void		check_context_timeout();
void		check_context_linelimit();

void		expand_context_action_macros();

#endif /* __STDC__ */
