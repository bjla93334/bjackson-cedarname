/* pfs.c -- Translate PFS file names.
   David Nichols
   December, 1991 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include "pfs.h"

extern char *malloc();
extern char *getenv();

#define TRUE			1
#define FALSE			0
#define MAXPREFIXLOOKUPS	10
#define MAXNAMELEN		1024

char *pfs_errorMsg;
static int pfsInited = FALSE;
static char *FSTranslateName();
static char *VUXTranslate();
static char *UXTranslate();
extern char *vermap_Translate();

struct PrefixEntry {
    struct PrefixEntry *next;
    char *name;
    int length;
    char *translation;
};
struct PrefixEntry *prefixes;

struct IPE {
    char *name;
    char *translation;
};
static struct IPE initialPrefixTable[] = {
    "/imagerfonts", "-ux:/project/pcedar2.0/imagerfonts",
    "/release", "/XeroxCedar/release",
    "/ux", "-ux:/",
    "/vux", "-vux:/",
    "/", "-ux:/",
    "/cedar", "/XeroxCedar/release",
    "/cedar10.1", "/XeroxCedar/release",
    "/XeroxCedar", "-vux:/project/cedar10.1/",
#if 1
    "/r", "-vermapa:/Source",
    "/rx", "-vermapx:/Source",
#endif

    NULL, NULL
};

struct FSEntry {
    char *name;			/* ux, vux, etc. */
    int length;			/* length of name */
    char *(*translateProc)();	/* routine to translate it */
};
static struct FSEntry fsTable[] = {
    "ux:", 0, UXTranslate,
    "vux:", 0, VUXTranslate,
    "vermapa:", 0, vermap_Translate,
    "vermapta:", 0, vermap_Translate,
    "vermapx:", 0, vermap_Translate,
    "vermaptx:", 0, vermap_Translate,
    NULL, 0, NULL
};


static char *strsav(s)
    char *s;
{
    char *p = malloc(strlen(s) + 1);

    strcpy(p, s);
    return p;
}

/*
   Translate a Cedar name to a Unix name.  Cedar names look like this:
 	 -fs:/name/name/name
   The -fs: part controls the interpretation of the rest (see the fsTable
   above).

   If the -fs: part is missing, then the name is run repeatedly through a
   prefix table to replace prefixes of the name with other prefixes.

   If the name doesn't begin with a / or a -, then it's assumed to be a
   Unix file relative to the current directory and is simply returned.
*/
char *pfs_TranslateName(name)
    char *name;
{
    char buf[MAXNAMELEN], buf2[MAXNAMELEN];
    int i, len;
    struct PrefixEntry *pe;
    struct FSEntry *fe;
    char *p;

    if (!pfsInited)
	InitPFS();
    pfs_errorMsg = NULL;
    for (i = 0; i < MAXPREFIXLOOKUPS; ++i) {
	if (*name == '-') {
	    p = strchr(name, ':');
	    if (p == NULL)
		return strsav(name);	/* not in right form */
	    for (fe = fsTable; fe->name != NULL; ++fe) {
		if (strncasecmp(name+1, fe->name, fe->length) == 0)
		    return fe->translateProc(fe, p+1);
	    }
	    return strsav(name);
	}
	if (*name != '/')
	    return strsav(name);
	/* Need to do prefix map lookups. */
	strcpy(buf, name);
	for (pe = prefixes; pe != NULL; pe = pe->next) {
	    if (strncasecmp(pe->name, buf, pe->length) == 0 &&
		(pe->length==1 || buf[pe->length] == 0 || buf[pe->length] == '/')) {
		strcpy(buf2, pe->translation);
		len = strlen(buf2);
		if (buf2[len - 1] == '/' && buf[pe->length] == '/')
		    buf2[--len] = 0;
		strcpy(buf2 + len, buf + pe->length);
		name = buf2;
		break;
	    }
	}
	if (pe == NULL) {
	    /* Fell off the end of the table; treat this as a Unix name. */
	    return strsav(buf);
	}
    }
    pfs_errorMsg = "Too many prefix map substitutions.";
    return NULL;
}

static void InsertPE(newpe)
     struct PrefixEntry *newpe;
{
    struct PrefixEntry *pe;
    struct PrefixEntry **lag = &prefixes;
    
    for (pe=prefixes, lag=&prefixes; pe != NULL; lag=&(pe->next), pe=pe->next) {
	if (newpe->length >= pe->length) {
	    newpe->next = pe;
	    *lag = newpe;
	    return;
	}
    }
    *lag = newpe;
}

static InitPFS()
{
    struct FSEntry *fe;
    struct IPE *ipe;
    struct PrefixEntry *pe;
    char *home, buf[1024];
    char command[1024], name[1024], translation[1024];
    char *xeroxCedar;
    FILE *f;

    for (fe = fsTable; fe->name != NULL; ++fe)
	fe->length = strlen(fe->name);
    for (ipe = initialPrefixTable; ipe->name != NULL; ++ipe) {
	pe = (struct PrefixEntry *) malloc(sizeof(*pe));
	pe->name = ipe->name;
	pe->length = strlen(ipe->name);
	pe->translation = ipe->translation;
	pe->next = NULL;
        InsertPE(pe);
    }
    xeroxCedar = getenv("XeroxCedar");
    if (xeroxCedar != NULL) {
        pe = (struct PrefixEntry *) malloc(sizeof(*pe));
        pe->name = strsav("/XeroxCedar");
        pe->length = strlen(pe->name);
        pe->translation = strsav(xeroxCedar);
        pe->next = NULL;
        InsertPE(pe);
    }
    home = getenv("HOME");
    if (home != NULL) {
	sprintf(buf, "%s/.cedar.pma", home);
	f = fopen(buf, "r");
	if (f != NULL) {
	    while (fgets(buf, sizeof(buf), f) != NULL) {
		if (sscanf(buf, "%s %s %s", command, name, translation) != 3
		    || strcmp(command, "pma") != 0)
		    continue;
		pe = (struct PrefixEntry *) malloc(sizeof(*pe));
		pe->name = strsav(name);
		pe->length = strlen(name);
		pe->translation = strsav(translation);
		pe->next = NULL;
		InsertPE(pe);
	    }
	    fclose(f);
	}
    }
    pfsInited = TRUE;
}

static char *UXTranslate(fe, name)
    struct FSEntry *fe;
    char *name;
    
{
    char *saveName;
    char *bangPos;
    saveName = strsav(name);
    if ((bangPos = strrchr(saveName, '!'))!=NULL) {
	if (bangPos >strrchr(saveName, '/')) *bangPos = 0;
    }
    return saveName;
}

/* Internal version values. */
#define HIGH	(-1)		/* want highest version */
#define LOW	(-2)		/* want lowest version */
#define NONE	(-3)		/* no version number */
#define UNKNOWN	(-4)		/* not known yet */

/*
   Deal with VUX version numbers.

   The -fs: and !version parts are optional.  If -fs: is -ux:, then the
   name is whatever follows the :.  If it is -vux:, then the name is
   translated to lower case, and the !version part is interpreted.  !n
   where n is an integer translates to .~n~, !l and !h (literally)
   translate to the highest and lowest version numbers, respectively.  If
   the !version is missing, !h is assumed.  If no versioned files are
   present, then an unversioned one is used.

   If the -fs: is missing, then the file name is run through the prefix map
   table and prefixes of the filename are substituted.  This can happen
   more than once.

 */
static char *VUXTranslate(fe, name)
    struct FSEntry *fe;
    char *name;
{
    char buf[MAXNAMELEN + 20];
    char *p;
    char *slash, *bang;
    int version;
    int bestVersion;
    DIR *dir;
    struct dirent *d;
    int len;

    /* First convert to lower case. */
    for (p = buf; *name != 0; ++p, ++name)
	*p = isupper(*name) ? tolower(*name) : *name;
    *p = 0;
    slash = strrchr(buf, '/');
    bang = strrchr(buf, '!');
    if (bang == NULL || (slash != NULL && slash > bang)) {
	/* No version number specified. */
	version = HIGH;
    }
    else {
	/* Parse version number. */
	*bang++ = 0;
	if (*bang == 'h' || *bang == 'H')
	    version = HIGH;
	else if (*bang == 'l' || *bang == 'L')
	    version = LOW;
	else {
	    version = atoi(bang);	/* save it */
	    /* Now make sure it's ok. */
	    for (p = bang; *p != 0; ++p) {
		if (!isdigit(*p)) {
		    pfs_errorMsg = "Bad version number.";
		    return NULL;
		}
	    }
	}
    }
    /* Now we need to find the version.  If it's explicit, we can just invent
       the name and we're done. */
    if (version >= 0) {
	p = buf + strlen(buf);
	sprintf(p, ".~%d~", version);
	return strsav(buf);
    }

    /* Ok, we need to scan the directory.  Open it, and leave slash pointing
       to the component. */
    if (slash == buf) {
	dir = opendir("/");
	++slash;
    }
    else if (slash == NULL) {
	dir = opendir(".");
	slash = buf;
    }
    else {
	*slash = NULL;
	dir = opendir(buf);
	*slash++ = '/';
    }
    if (dir == NULL) {
	pfs_errorMsg = "Can't open directory to find version.";
	return NULL;
    }
    /* Do the actual scan. */
    len = strlen(slash);
    bestVersion = UNKNOWN;
    while ((d = readdir(dir)) != NULL) {
	if (strncmp(d->d_name, slash, len) == 0) {
	    if (d->d_name[len] == 0) {
		/* A file that matches with no version number. */
		if (bestVersion == UNKNOWN)
		    bestVersion = NONE;
	    }
	    else if (d->d_name[len] == '.'
		     && d->d_name[len + 1] == '~'
		     && d->d_name[d->d_namlen - 1] == '~') {
		/* A match if the stuff between the ~'s is numeric. */
		int ok = 1, i, v;
		for (i = len + 2; i < d->d_namlen - 1; ++i) {
		    if (!isdigit(d->d_name[i]))
			ok = 0;
		}
		if (ok) {
		    v = atoi(&d->d_name[len + 2]);
		    if (version == HIGH) {
			if (bestVersion < 0 || v > bestVersion)
			    bestVersion = v;
		    }
		    else {
			if (bestVersion < 0 || v < bestVersion)
			    bestVersion = v;
		    }
		}
	    }
	}
    }
    if (bestVersion == UNKNOWN) {
	pfs_errorMsg = "Can't find valid version.";
	return NULL;
    }
    else if (bestVersion == NONE)
	return strsav(buf);
    else {
	p = buf + strlen(buf);
	sprintf(p, ".~%d~", bestVersion);
	return strsav(buf);
    }
}
