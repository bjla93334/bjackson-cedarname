/* pfs.h -- Translate PFS file names.
   David Nichols
   December, 1991 */

/* Takes a char * and returns an allocated string with the real Unix file
   name that corresponds to the Cedar file name passed in.  If something
   goes wrong, it returns NULL and pfsErrorMsg tells what went wrong. */
extern char *pfs_TranslateName(/* char *name */);

extern char *pfs_errorMsg;
