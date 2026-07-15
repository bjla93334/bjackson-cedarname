/* cedarname.c -- Translate Cedar file names to Unix file names.
   David Nichols
   December, 1990 */

#include <stdio.h>
#include "pfs.h"

main(argc, argv)
    int argc;
    char *argv[];
{
    int i;
    char *p;

    for (i = 1; i < argc; ++i) {
	p = pfs_TranslateName(argv[i]);
	if (p == NULL) {
	    fprintf(stderr, "%s\n", pfs_errorMsg);
	    p = argv[i];
	}
	printf("%s\n", p);
    }
}
