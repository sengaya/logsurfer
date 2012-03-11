/*
 * print time information
 *
 * V1.0 - 09 Jan 96  DFN-CERT
 *	written by Wolfgang Ley
 *
 * usage: prtime [-o] seconds
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>


/*
 * print usage information and exit
 */
static void usage(progname)
char	*progname;
{
	(void) fprintf(stderr, "usage: %s [-o] seconds\n" ,progname);
	exit(1);
}


/*
 * main program
 */
int main(argc, argv)
int	argc;
char	**argv;
{
	char		*progname;	/* name of this program		*/
	int		opt;		/* used for options processing	*/
	extern char	*optarg;	/* option argument		*/
	extern int	optind;		/* option index			*/
	int		pr_offset=0;	/* print only the offset?	*/
	time_t		secs, now;	/* seconds (argument)		*/

	if ( (progname=strrchr(argv[0], '/')) == NULL )
		progname=argv[0];
	else
		progname++;

	while ( (opt = getopt(argc, argv, "oh")) != EOF )
		switch(opt) {
		case 'o':
			pr_offset=1;
			break;
		case 'h':
		default:
			usage(progname);
			break;
		}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage(progname);

	if ( (secs=atol(argv[0])) == 0 ) {
		(void) fprintf(stderr, "%s: use a positive integer as argument (secs)\n", progname);
		exit(2);
	}

	if ( pr_offset ) {
		time(&now);
		(void) printf("%ld seconds offset\n", secs-now);
	}
	else {
		(void) printf("%s", ctime(&secs));
	}

	return(0);
}

