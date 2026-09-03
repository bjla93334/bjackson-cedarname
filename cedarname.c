/**
    cedarname.c -- Translate Cedar file names to Unix file names.
    David Nichols, December 1990
    Bill Jackson, July 2026
*/

#include <stdio.h>
#include <strings.h>
#include "pfs.h"

#define OPT_INIT
#include "options.h"

int main(int argc, char *argv[]) {
    int rc = 0;

/*
    o_flags = (1 << OPT_DUMPENTRY);
    if (opt_set(OPT_DUMPENTRY)) {
        printf("OPT_DUMPENTRY\n");
        return 0;
    }
*/

    for (int i = 1; i < argc; ++i) {
        char *pfs_path = argv[i];
        int found = -1;
        for (int j = 0; options[j] != NULL; j++) {
            if (strcasecmp(pfs_path, options[j]) == 0) {
                found = j;
                break;
            }
        }
        if (found != -1) {
            o_flags ^= (1 << found); // yeah, primitive, sloppy
            continue;
        }
        char *p = pfs_TranslateName(pfs_path);
	if (p == NULL) {
	    fprintf(stderr, "%s : %s\n", pfs_path, pfs_errorMsg);
            rc = 1;
	    continue;
	}
	printf("%s\n", p);
    }

    if (opt_set(OPT_DUMP_ALL)) { DumpAll(1); }
    // OPT_DUMP_ENTRY,
    if (opt_set(OPT_DUMP_INDEX)) { DumpIndex(); }
    if (opt_set(OPT_DUMP_SORTED)) { DumpSorted(1); }
    if (opt_set(OPT_DUMP_PREFIXMAP)) { DumpPrefixMap(); }
    if (opt_set(OPT_DUMP_STAB)) { DumpStab(); }

    {
        extern char *vermap_LookupStamp(int stamp);
        int duncan = 0x4ab6a9f8; // /r/Tioga.tip - stamp: 4ab6a9f8 4ab6 num: 19126 a9f8 hi: 43512
        char *ifs_name = vermap_LookupStamp(duncan);
        printf("duncan : %s %d 0x%x\n", ifs_name, duncan, duncan);
    }

    return rc;
}
