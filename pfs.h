/* pfs.h
    Translate PFS file names.
    David Nichols, December 1991
    Bill Jackson, July 2026
*/

/**
    returns an allocated string with an expanded pathname for the PFS name passed in.
    When something goes wrong, returns NULL and pfsErrorMsg tells what went wrong.
*/
extern char *pfs_TranslateName(char *name);
extern char *pfs_errorMsg;
