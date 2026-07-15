/**
    cedarname.c -- Translate Cedar file names to Unix file names.
    David Nichols, December 1990
    Bill Jackson, July 2026
*/

#include <stdio.h>
#include "pfs.h"

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        char *pfs_path = argv[i];
        char *p = pfs_TranslateName(pfs_path);
	if (p == NULL) {
	    fprintf(stderr, "%s : %s\n", pfs_path, pfs_errorMsg);
	    continue;
	}
	printf("%s\n", p);
    }
}
