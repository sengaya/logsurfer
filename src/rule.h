/*
 * rule.h
 *
 * header file: routines to handle rules (create, delete, ...)
 */

#if __STDC__

struct rule	*create_rule();

void		destroy_rule(struct rule *);
void		unlink_rule(struct rule *);

struct rule	*parse_rule(char *);
void		dynamic_add_rule(struct rule *);
void		add_rule_behind(struct rule *, struct rule *);
void		add_rule_before(struct rule *, struct rule *);

void		process_rule(struct rule *);

void		check_rule_timeout();

#else /* __STDC__ */

struct rule	*create_rule();

void		destroy_rule();
void		unlink_rule();

struct rule	*parse_rule();
void		dynamic_add_rule();
void		add_rule_behind();
void		add_rule_before();

void		process_rule();

void		check_rule_timeout();

#endif /* __STDC__ */

