/* vermap.c -- Routines to interpret Cedar version maps.
   David Nichols
   December 1991 */

#include <stdio.h>
#include <string.h>
#include "pfs.h"

extern char *malloc();

#define ShortKey	19850206
#define LongKey		19900710

struct Stamp {
    short lo, num, hi, extra;
};
struct MapEntry {
    struct Stamp stamp;
    long created;		/* cedar time value */
    long index;			/* index of first char of long name */
};
struct Map {
    long len;			/* number of elements in each of next two  */
    long *shortNames;		/* index of an entry, in shortname order */
    struct MapEntry *entries;	/* the map entries */
    long nChars;		/* number of chars in names */
    char *names;		/* all the names */
};

#define IsDelim(c)	((c) == '[' || (c) == ']' || (c) == '<' || \
			 (c) == '>' || (c) == '/')

/* Read the version map from disk.  Assumes endian match with data. */
static struct Map *ReadMap(name)
    char *name;			/* file name of map */
{
    FILE *f;
    long key, len, nChars;
    struct Map *map;
    int c;
    unsigned short a[7];	/* for dealing with short form */
    int c1, c2;
    int i;

    f = fopen(name, "r");
    if (f == NULL)
	return NULL;
    /* File starts with three ASCII integers and a CR. */
    if (fscanf(f, "%ld %ld %ld", &key, &len, &nChars) != 3) {
	fclose(f);
	return NULL;
    }
    c = getc(f);
    if (c != '\r') {
	fclose(f);
	return NULL;
    }
    /* Only long format for now. */
    if (key != LongKey && key != ShortKey) {
	fclose(f);
	return NULL;
    }
    map = (struct Map *) malloc(sizeof(*map));
    map->entries = (struct MapEntry *)
      malloc((len + 1) * sizeof(struct MapEntry));
    map->shortNames = (long *) malloc(len * sizeof(long));
    map->names = (char *) malloc(nChars);
    map->len = len;
    map->nChars = nChars;
    if (key == LongKey) {
	if (fread(map->entries, sizeof(struct MapEntry), len, f) != len
	    || fread(map->shortNames, sizeof(long), len, f) != len)
	    goto bad;
    }
    else {
	for (i = 0; i < len; ++i) {
	    if (fread(a, sizeof(a), 1, f) != 1)
		goto bad;
	    map->entries[i].index = ((long) a[6] << 16) | (long) a[5];
	    /* others don't matter */
	}
	for (i = 0; i < len; ++i) {
	    c1 = getc(f);
	    c2 = getc(f);
	    if (c1 == EOF || c2 == EOF)
		goto bad;
	    map->shortNames[i] = (c1 << 8) | c2;
	}
    }
    if (fread(map->names, 1, nChars, f) != nChars)
	goto bad;
    /* Make it easy to find the end of a name. */
    map->entries[len].index = nChars;
    fclose(f);
    return map;
bad:
    free(map->names);
    free(map->shortNames);
    free(map->entries);
    free(map);
    fclose(f);
    return NULL;
}

/* Do the binary search in the tree. */
static int ShortNameFind(map, name)
    struct Map *map;
    char *name;
{
    int lo = 0;
    int hi = map->len - 1;
    int index;
    int r;

    while (lo <= hi) {
	index = (lo + hi) / 2;
	r = Compare(map, name, map->shortNames[index]);
	if (r < 0) {
	    if (lo == index)
		break;
	    hi = index - 1;
	}
	else if (r > 0) {
	    if (hi == index)
		break;
	    lo = index + 1;
	}
	else			/* equal */
	    return map->shortNames[index];
    }
    return -1;
}

static int Compare(map, name, index)
    struct Map *map;
    char *name;
    int index;
{
    int start, end;
    int ver, snStart;
    int c;
    int i;

    start = map->entries[index].index;
    end = map->entries[index + 1].index - 1;
    /* Search backward for beginning of short name or version marker. */
    ver = snStart = 0;
    for (i = end - 1; i >= start; --i) {
	c = map->names[i];
	if (ver == 0 && c == '!')
	    ver = i;
	if (snStart == 0 && IsDelim(c)) {
	    snStart = i + 1;
	    break;
	}
    }
    if (ver != 0)
	end = ver;
    if (snStart != 0)
	start = snStart;
    return strncasecmp(name, map->names + start, end - start);
}

static char *vermap_Lookup(map, name)
    struct Map *map;
    char *name;
{
    int i;
    int start, end;
    char buf[1024], buf2[1024];

    if (map == NULL) {
	pfs_errorMsg = "Can't find version map.";
	return NULL;
    }
    i = ShortNameFind(map, name);
    if (i == -1) {
	pfs_errorMsg = "Can't find in version map.";
	return NULL;
    }
    /* Found it. */
    start = map->entries[i].index;
    end = map->entries[i + 1].index - 1;
    strncpy(buf, map->names + start, end - start);
    buf[end - start] = 0;
    Slashify(buf2, buf);
    return pfs_TranslateName(buf2);
}


/* Simplistic routine to convert old-style Cedar file names to new-style. */
static Slashify(to, from)
    char *to;
    char *from;
{
    int c, lastC = 0;

    for (; *from != 0; ++from) {
	c = *from;
	if (IsDelim(c)) {
	    c = '/';
	    if (lastC == '/')
		continue;
	}
	*to++ = lastC = c;
    }
    *to++ = 0;
}

struct FSEntry {
    char *name;			/* ux, vux, etc. */
    int length;			/* length of name */
    char *(*translateProc)();	/* routine to translate it */
};

static char *cedarMapName = "/Cedar/CedarVersionMap/CedarSource.VersionMap";
static struct Map *cedarMap = NULL;

char *vermap_Translate(fe, name)
    struct FSEntry *fe;
    char *name;
{
    char *p;
    char *res;

    p = strrchr(name, '/');
    if (p == NULL)
	p = name;
    else
	++p;
    {
	if (cedarMap == NULL)
	    cedarMap = ReadMap(pfs_TranslateName(cedarMapName));
	res = vermap_Lookup(cedarMap, p);
    }
    return res;
}
