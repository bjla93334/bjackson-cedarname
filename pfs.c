#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int atoi(const char *);
extern void *malloc(long unsigned int);
extern char *getenv(const char *);

#include "pfs.h"

#define TRUE			1
#define FALSE			0
#define MAXPREFIXLOOKUPS	10
#define MAXNAMELEN		1024

char *pfs_errorMsg;

static int pfsInited = FALSE;

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
    char *(*translateProc)(struct FSEntry *, char *);	/* routine to translate it */
};

// static char *FSTranslateName();
static char *VUXTranslate(struct FSEntry *fe, char *name);
static char *UXTranslate(struct FSEntry *fe, char *name);
extern char *vermap_Translate(struct FSEntry *fe, char *name);

static struct FSEntry fsTable[] = {
    "ux:", 0, UXTranslate,
    "vux:", 0, VUXTranslate,
    "vermapa:", 0, vermap_Translate,
    "vermapta:", 0, vermap_Translate,
    "vermapx:", 0, vermap_Translate,
    "vermaptx:", 0, vermap_Translate,
    NULL, 0, NULL
};

static char *strsav(char *s) {
    char *p = malloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

static void InitPFS();

/**
    Translate a Cedar pathname to a Unix pathname.
    Cedar pathnames look like this:

             -fs:/name/name/name

    The -fs: part controls the interpretation of the rest (see the fsTable above).

    When the -fs: part is absent, the value is run repeatedly through the prefix table
    until a fully resolved pathname has been constructed.

    For the case where the resolved pathname doesn't begin with / or -,
    it's assumed to be a file relative to the current directory.
*/
char *pfs_TranslateName(char *name) {
    if (!pfsInited) InitPFS();

    pfs_errorMsg = NULL;

    char buf[MAXNAMELEN];
    char buf2[MAXNAMELEN];
    for (int i = 0; i < MAXPREFIXLOOKUPS; ++i) {
        if (*name == '-') {
            char *p = strchr(name, ':');
            if (p == NULL) {
                fprintf(stderr, "%s : %s\n", name, "bad form");
                return strsav(name);	/* not in right form */
            }

            for (struct FSEntry *fe = fsTable; fe->name != NULL; ++fe) {
                if (strncasecmp(name+1, fe->name, fe->length) == 0) {
                    // printf("%s : trying %s\n", name, fe->name);
                    return fe->translateProc(fe, p+1);
                }
            }

            return strsav(name);
        }

        if (*name != '/') { return strsav(name); }

        /* Need to do prefix map lookups. */

        strcpy(buf, name);

        struct PrefixEntry *expanded = NULL;
        for (struct PrefixEntry *pe = prefixes; pe != NULL; pe = pe->next) {
            // printf("prefix map expansion loop : %s\n", pe->name);
            if ((strncasecmp(pe->name, buf, pe->length) == 0)
            && (
                (pe->length == 1)
                || (buf[pe->length] == 0)
                || (buf[pe->length] == '/')
                )
            ) {
                strcpy(buf2, pe->translation);
                int len = strlen(buf2);
                if (buf2[len - 1] == '/' && buf[pe->length] == '/') buf2[--len] = 0;
                strcpy(buf2 + len, buf + pe->length);
                name = buf2;
                expanded = pe;
                // printf("prefix map match : %s\n", expanded->name);
                break;
            }
        }

        if (expanded == NULL) { return strsav(buf); } // other more to match

        /* Fell off the end of the table; treat this as a relative name. */
    }

    pfs_errorMsg = "Too many prefix map substitutions.";
    return NULL;
}

static void InsertPE(struct PrefixEntry *newpe) {
    struct PrefixEntry *pe = prefixes;
    struct PrefixEntry **lag = &prefixes;
    
    for ( ; pe != NULL; lag=&(pe->next), pe=pe->next) {
        if (newpe->length >= pe->length) {
            newpe->next = pe;
            *lag = newpe;
            return;
        }
    }
    *lag = newpe;
}

static void DumpPrefixMap() {
    for (struct PrefixEntry *pe = prefixes; pe != NULL; pe=pe->next) {
        printf("%s(%d) %s\n", pe->name, pe->length, pe->translation);
    }
}

static void InitPFS() {
    for (struct FSEntry *fe = fsTable; fe->name != NULL; ++fe) fe->length = strlen(fe->name);

    for (struct IPE *ipe = initialPrefixTable; ipe->name != NULL; ++ipe) {
        struct PrefixEntry *pe = (struct PrefixEntry *) malloc(sizeof(*pe));
        pe->name = ipe->name;
        pe->length = strlen(ipe->name);
        pe->translation = ipe->translation;
        pe->next = NULL;
        InsertPE(pe);
    }

    char *xeroxCedar = getenv("XeroxCedar");
    if (xeroxCedar != NULL) {
        struct PrefixEntry *pe = (struct PrefixEntry *) malloc(sizeof(*pe));
        pe->name = strsav("/XeroxCedar");
        pe->length = strlen(pe->name);
        pe->translation = strsav(xeroxCedar);
        pe->next = NULL;
        InsertPE(pe);
    }

    char *home = getenv("HOME");
    if (home != NULL) {
        char buf[1024];
        sprintf(buf, "%s/.cedar.pma", home);
        FILE *f = fopen(buf, "r");
        if (f != NULL) {
            while (fgets(buf, sizeof(buf), f) != NULL) {
                char command[1024];
                char name[1024];
                char translation[1024];
                int count = sscanf(buf, "%s %s %s", command, name, translation);

                if (count  != 3 || strcmp(command, "pma") != 0) continue;

                struct PrefixEntry *pe = (struct PrefixEntry *) malloc(sizeof(*pe));
                pe->name = strsav(name);
                pe->length = strlen(name);
                pe->translation = strsav(translation);
                pe->next = NULL;
                InsertPE(pe);
            }
            fclose(f);
        }
    }

    if (0 == 1) DumpPrefixMap(); // DEBUG, should this be a feature ?
    pfsInited = TRUE;
}

static char *UXTranslate(struct FSEntry *fe, char *name) {
    char *saveName = strsav(name);
    char *bangPos = strrchr(saveName, '!');
    if (bangPos != NULL) {
        if (bangPos > strrchr(saveName, '/')) *bangPos = 0;
    }
    return saveName;
}

/* Internal version values. */
#define HIGH	(-1)		/* want highest version */
#define LOW	(-2)		/* want lowest version */
#define NONE	(-3)		/* no version number */
#define UNKNOWN	(-4)		/* not known yet */

/**
    Deal with VUX version numbers.

    The -fs: and !version parts are optional.

    When -fs: is -ux:, the pathname is whatever follows the :.
    When it is -vux:, the pathname is translated to lower case, and the !version part is interpreted.

    !n where n is an integer translates to .~n~,
    !l and !h (literally) translate to the highest and lowest version numbers, respectively.
    When the !version is missing, !h is assumed.

    When no versioned files are present, an unversioned one is used.

    When  -fs: is missing, the pathname is run through the prefix map table
    and prefixes of the filename are substituted.
    This can happen more than once.
 */
static char *VUXTranslate(struct FSEntry *fe, char *name) {
    /* First convert to lower case. */
    char buf[MAXNAMELEN + 20];
    char *p;
    for (p = buf; *name != 0; ++p, ++name) {*p = isupper(*name) ? tolower(*name) : *name; }
    *p = 0;

    int version;

    char *slash = strrchr(buf, '/');
    char *bang = strrchr(buf, '!');
    if (bang == NULL || ((slash != NULL) && (slash > bang))) {
        /* No version number specified. */
        version = HIGH;
    }
    else {
        /* Parse version number. */
        *bang++ = 0;
        if (*bang == 'h' || *bang == 'H') version = HIGH;
        else if (*bang == 'l' || *bang == 'L') version = LOW;
        else {
            version = atoi(bang);	/* save it */
            /* Now make sure it's ok. */
            for (char *p = bang; *p != 0; ++p) {
                if (!isdigit(*p)) { pfs_errorMsg = "Bad version number."; return NULL; }
            }
        }
    }

    /*
      Now we need to find the version.
      When it's explicit, we can just invent the name and we're done.
    */
    if (version >= 0) {
        char *p = buf + strlen(buf);
        sprintf(p, ".~%d~", version);
        return strsav(buf);
    }

    /*
      need to scan the directory
      Open it, and leave slash pointing to the component.
    */
    DIR *dir;
    if (slash == buf) {
        dir = opendir("/");
        ++slash;
    }
    else if (slash == NULL) {
        dir = opendir(".");
        slash = buf;
    }
    else {
        *slash = 0; // NULL;
        dir = opendir(buf);
        *slash++ = '/';
    }

    if (dir == NULL) { pfs_errorMsg = "Can't open directory to find version."; return NULL; }

    /* Do the actual scan. */
    int len = strlen(slash);
    int bestVersion = UNKNOWN;
    struct dirent *d;
    while ((d = readdir(dir)) != NULL) {
        if (strncmp(d->d_name, slash, len) == 0) {
            if (d->d_name[len] == 0) {
                /* A file that matches with no version number. */
                if (bestVersion == UNKNOWN) bestVersion = NONE;
            }
            else if (d->d_name[len] == '.'
                 && d->d_name[len + 1] == '~'
                 // && d->d_name[d->d_namlen - 1] == '~'
                 && d->d_name[_D_EXACT_NAMLEN(d) - 1] == '~'
            ) {
                /* A match if the stuff between the ~'s is numeric. */
                int ok = 1;
                // for (int i = len + 2; i < d->d_namlen - 1; ++i)
                for (int i = len + 2; i < _D_EXACT_NAMLEN(d) - 1; ++i) {
                    if (!isdigit(d->d_name[i])) ok = 0;
                }

                if (ok) {
                    int v = atoi(&d->d_name[len + 2]);
                    if (version == HIGH) {
                        if (bestVersion < 0 || v > bestVersion) bestVersion = v;
                    }
                    else {
                        if (bestVersion < 0 || v < bestVersion) bestVersion = v;
                    }
                }
            }
        }
    }

    if (bestVersion == UNKNOWN) { pfs_errorMsg = "Can't find valid version."; return NULL; }
    if (bestVersion == NONE) return strsav(buf);

    {
        char *p = buf + strlen(buf);
        sprintf(p, ".~%d~", bestVersion);
        return strsav(buf);
    }
}
