/*
 * report.h
 *
 * header file: routines to handle reports (create, collect, ...)
 */

#if __STDC__

void	make_report(struct action_tokens *, struct context_body *);
void	add_to_report(struct context_body *, struct context_body *);

#else /* __STDC__ */

void	make_report();
void	add_to_report();

#endif /* __STDC__ */
