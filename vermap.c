/* vermap.c
    Routines to interpret Cedar version maps.
    David Nichols, December 1991
    Bill Jackson, July 2026
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pfs.h"
#include "options.h"

extern void *malloc(long unsigned int);

#define ShortKey	19850206
#define LongKey		19900710

/*
   These are binary data formats dictated by the Cedar runtime that generated the version map
   Currently, this is 32-bit Big Endian. Unfortunately, the file is not explicitly self-identifying.
 */
struct __attribute__((packed)) Stamp {
    uint16_t lo;
    uint16_t num;
    uint16_t hi;
    uint16_t extra;
};

struct __attribute__((packed)) MapEntry {
    struct Stamp stamp;
    uint32_t created;		/* cedar time value */
    uint32_t index;		/* index of first char of long name */
};

struct Map {
    long len;			/* number of elements in each of next two  */
    uint32_t *shortNames;	/* index of an entry, in shortname order */
    struct MapEntry *entries;	/* the map entries */
    long nChars;		/* number of chars in names */
    char *names;		/* all the names */
    uint32_t *locationIndex;    /* 'btree' of location names (not from map) */
};

static void fetchCanon(struct Map *map, int i, char *buf) {
    int start = map->entries[i].index;
    int end = map->entries[i + 1].index - 1;
    int nbytes = end - start;
    // assert(nbytes < 1024);
    strncpy(buf, map->names + start, nbytes);
    buf[end - start] = 0;
}

// FIXME : totally broken!
// routines that may be useful when the binary format changes
static void DumpTextAt(struct Map *map, int start, int slen) {
    char *table = map->names;
    for (int j = 0; j < slen; j++) {
        char ch = table[start+j];
        printf("%c", ch);
    }
    printf("\n");
}

static void DumpTextFor(struct Map *map, int i, int swap) {
    struct MapEntry *mep = &(map->entries[i]);
    struct MapEntry *after = &(map->entries[i+1]);

    int start = (swap) ? be32toh(mep->index) : mep->index;
    int end = (swap) ? be32toh(after->index) : after->index;
    int slen = (end - 1) - start;
    DumpTextAt(map, start, slen);
}

// time_t epoch_time = 1784419200; (e.g., Tuesday, August 18, 2026)
static void format_utc(time_t epoch_time, char *buffer, size_t buflen) {
    struct tm utc_time;
    gmtime_r(&epoch_time, &utc_time);
    strftime(buffer, buflen, "%Y-%m-%d %H:%M:%S UTC", &utc_time);
/*
    // tm_year since 1900
    // tm_mon 0-11
    printf("%04d-%02d-%02d %02d:%02d:%02d UTC",
       utc_time.tm_year + 1900,
       utc_time.tm_mon + 1,
       utc_time.tm_mday,
       utc_time.tm_hour,
       utc_time.tm_min,
       utc_time.tm_sec
    );
*/
}

// don't think too hard about what 'swap' means
static void DumpEntry(struct Map *map, int i, int swap) {
    uint32_t stab = (swap) ? map->shortNames[i] : be32toh(map->shortNames[i]); // arg to Compare()

    struct MapEntry *mep = &(map->entries[i]);
    uint32_t created = (swap) ? mep->created : be32toh(mep->created);
    uint32_t index = (swap) ? mep->index : be32toh(mep->index); // index into string table
    uint16_t lo = (swap) ? mep->stamp.lo : be16toh(mep->stamp.lo);
    uint16_t num = (swap) ? mep->stamp.num : be16toh(mep->stamp.num);
    uint16_t hi = (swap) ? mep->stamp.hi : be16toh(mep->stamp.hi);
    uint16_t extra = (swap) ? mep->stamp.extra : be16toh(mep->stamp.extra);

    // leap year ??
    uint64_t utc = (-24*60*60) + (-2*365*24*60*60) + (uint64_t) created;
    /*
       'created' is a BasicTime - seconds since 1901 / 1968 ; so wrong unix epoch
       Alto: 1901 to 2036
       XNS: 1968 to 2103 (Alto offset to 1968)

        1991-05-13 14:05:58.000000000 -0700 /r/Tioga.tip
       -2208988800 UTC: Monday, January 1, 1900 at 12:00:00 AM
       -63158400 UTC: Monday, January 1, 1968 at 12:00:00 AM
       0 UTC: Thursday, January 1, 1970 at 12:00:00 AM
    */
    char buffer[80];
    format_utc(utc, buffer, sizeof(buffer));
    char *calendar = buffer;

    char canon[1024];
    fetchCanon(map, i, canon);

    // FIXME : perhaps use JSON and have this be a feature ?
    printf(
        " i: %d"
        "\ncanon: %s"
        "\n%08x stab: %d"
        "\n%08x created: %d"
        "\n%08lx utc: %ld (%s)"
        "\n%08x index: %d"
"\nstamp:"
        " %04x%04x"
        " %04x num: %d"
        " %04x hi: %d"
        " %04x lo: %d"
        " %04x extra: %d"
        "\n",
        i,
        canon,
        stab, stab,
        created, created,
        utc, utc, calendar,
        index, index,
        num, hi,
        num, num,
        hi, hi,
        lo, lo,
        extra, extra
    );
}

// Version Maps use Alto IFS syntax : [Server]<Directory>SubDir>Base.ext!N

#define IsDelim(c)	((c) == '[' || (c) == ']' || (c) == '<' || (c) == '>' || (c) == '/')

/*
  There's obvious approaches to use here.
  We could "wrap getters" for machine dependent files, or
  rather than bulk reading, we could convert the stream (ala rpc) while pulling in the data,
  or (quick and dirty), read in bulk and swap everything in place before it's used

  This could also be smarter abouth whether swapping is needed or not.
 */
#include <endian.h>
static void FixMapEndian(struct Map *map) {
    for (int i = 0; i < map->len; i++) {
        // DumpEntry(map, i, 1); // print predicted outcome

        // pull out biggee-values;
        struct MapEntry *mep = &(map->entries[i]);
        uint32_t name_index = be32toh(map->shortNames[i]);

        uint32_t created = be32toh(mep->created);
        uint32_t index = be32toh(mep->index); // index into string table
        uint16_t lo = be16toh(mep->stamp.lo);
        uint16_t num = be16toh(mep->stamp.num);
        uint16_t hi = be16toh(mep->stamp.hi);
        uint16_t extra = be16toh(mep->stamp.extra);

        // copy back host-order values;
        map->shortNames[i] = name_index;
        mep->created = created;
        mep->index = index;
        mep->stamp.lo = lo;
        mep->stamp.num = num;
        mep->stamp.hi = hi;
        mep->stamp.extra = extra;

        // DumpEntry(map, i, 0); exit(0); // print outcome
    }
}

// wasn't being very deliberate when making these changes - hubris!
static void DumpSome(struct Map *map, int swap) {
    uint32_t *snp = map->shortNames;
    char *table = map->names;

    int which = 7629; // 7634 total
    which = 0;
    printf("DumpSome %d\n", swap);
    for (int i = 0; i < 5; i++) {
        DumpEntry(map, which + i, swap);
        // DumpTextFor(map, which + i, swap);
    }
    printf("\n");
}

// insertion bubble-sort
void insertLocation(struct Map *map, int *table, int len) {
    int newbie_index = len;
    char newbie[1024];
    fetchCanon(map, newbie_index, newbie);
    // printf("%d %s\n", newbie_index, newbie);

    char opponent[1024];

    table[len] = newbie_index; // insert at 'bottom'
    for (int finger = len; finger > 0 ; finger--) {
        int opponent_index = table[finger-1];
        fetchCanon(map, opponent_index, opponent);
        int placing = strcmp(newbie, opponent); // who cares about matches ??
        // printf("%d %s %d\n", newbie_index, newbie, placing);

        // if (newbie > opponent) break;
        if (placing > 0) break;

        // swap these
        table[finger] = opponent_index;
        table[finger-1] = newbie_index;
    }
}

/* Read the version map from disk.  Assumes endian match with data. */
static struct Map *ReadMap(char *cedarMapIFSName) {
    char *name = pfs_TranslateName(cedarMapIFSName);
    FILE *fd = fopen(name, "r");
    if (fd == NULL) { perror(name); }
    if (fd == NULL) return NULL;

    /* File starts with three ASCII integers and CR. */
    long key, len, nChars;
    int count = fscanf(fd, "%ld %ld %ld", &key, &len, &nChars);
    if (count != 3) { fclose(fd); return NULL; }
    // fprintf(stderr, "%s : %s\n", name, "count : broken first line?");

    int c = getc(fd);
    if (c != '\r') { fclose(fd); return NULL; } // yes, CR, not LF
    // fprintf(stderr, "%s : %s\n", name, "endl : broken first line?");

    /* Only long format for now. */
    if (key != LongKey && key != ShortKey) { fclose(fd); return NULL; }
    // fprintf(stderr, "%s : %s(%ld)\n", name, "bad key", key);

    // another possible feature : dump version map stats:
    // printf("first line : %ld %ld %ld\n", key, len, nChars);

    struct Map *map = (struct Map *) malloc(sizeof(*map));
    map->entries = (struct MapEntry *) malloc((len + 1) * sizeof(struct MapEntry));
    map->shortNames = (uint32_t *) malloc(len * sizeof(uint32_t));
    map->names = (char *) malloc(nChars);
    map->len = len;
    map->nChars = nChars;

    map->locationIndex = (int *) malloc(len * sizeof(int));

    // printf("map : %ld %ld\n", map->len, map->nChars);

    // table of MapEntry
    // table of shortNames
    // stab
    if (key == LongKey) {
        long xx = fread(map->entries, sizeof(struct MapEntry), len, fd);
        // printf("LongKey entries : %ld\n", xx);
        if (xx != len) goto bad;

        long yy = fread(map->shortNames, sizeof(uint32_t), len, fd);
        // printf("LongKey shortNames : %ld\n", yy);
        if (yy != len) goto bad;

        // DumpSome(map, 1);
        FixMapEndian(map);
        // DumpSome(map, 0);
    }
    else {
// FIXME: byte order? / dead code?
        unsigned short a[7];	/* for dealing with short form */
	for (int i = 0; i < len; ++i) {
	    if (fread(a, sizeof(a), 1, fd) != 1) goto bad;
	    map->entries[i].index = ((long) a[6] << 16) | (long) a[5];
	    /* others : a[0-4] don't matter */
	}

	for (int i = 0; i < len; ++i) {
	    int c1 = getc(fd);
	    int c2 = getc(fd);
	    if (c1 == EOF || c2 == EOF) goto bad;
	    map->shortNames[i] = (c1 << 8) | c2;
	}
    }

    long zz = fread(map->names, 1, nChars, fd);
    if (zz != nChars) {
        goto bad;
    }

    /*
    // file length should match the amount of data we've read
    if (debug) {
        long total = 
            (len * sizeof(struct MapEntry))
            + (len * sizeof(uint32_t))
            + nChars
        ;
        printf("names : %ld %ld %ld %ld %ld : %ld\n", 
            sizeof(struct MapEntry), sizeof(uint32_t),
            map->len, map->nChars,
            total, zz
        );
    }
    */

    /*
      Make it easy to find the end of name.
      an extra entry was allocated just for this
     */
    map->entries[len].index = nChars;
    fclose(fd);

    // build locationIndex, ordered appropriately
    for (int i = 0; i < map->len; i++) {
        insertLocation(map, map->locationIndex, i);
    }
    return map;
bad:
    free(map->names);
    free(map->shortNames);
    free(map->entries);
    free(map->locationIndex);
    free(map);
    fclose(fd);
    // fprintf(stderr, "%s : map read error\n", name);
    return NULL;
}

static int Compare(struct Map *map, char *name, int index);

/* Do the binary search in the tree. */
static int ShortNameFind(struct Map *map, char *name) {
    int lo = 0;
    int hi = map->len - 1;

    while (lo <= hi) {
	int index = (lo + hi) / 2;

        // printf("ShortNameFind %d %d %d\n", lo, hi, index);
	int r = Compare(map, name, map->shortNames[index]);
	if (r < 0) {
	    if (lo == index) break;
	    hi = index - 1;
	}
	else if (r > 0) {
	    if (hi == index) break;
	    lo = index + 1;
	}
	else /* equal */
	    return map->shortNames[index];
    }

    return -1;
}

static int Compare(struct Map *map, char *name, int index) {
    int start = map->entries[index].index;
    int end = map->entries[index + 1].index - 1;

    // printf("Compare %d %d %d\n", start, end, 0);

    /* Search backward for beginning of short name or version marker. */
    int ver = 0;
    int snStart = 0;
    for (int i = end - 1; i >= start; --i) {
	int c = map->names[i];
	if (ver == 0 && c == '!') ver = i;
	if (snStart == 0 && IsDelim(c)) { snStart = i + 1; break; }
    }

    if (ver != 0) end = ver;
    if (snStart != 0) start = snStart;

    // printf("Compare2 %d %d %d\n", start, end, end - start);
    int xlen = end - start;

    // DumpTextAt(map, start, xlen);
    return strncasecmp(name, map->names + start, end - start);
}

static void Slashify(char *to, char *from);

static char *vermap_Lookup(struct Map *map, char *name) {
    if (map == NULL) { pfs_errorMsg = "Can't find, no version map."; return NULL; }

    int i = ShortNameFind(map, name);
    if (i == -1) { pfs_errorMsg = "Can't find in version map."; return NULL; }

    /* Found it. */
    if (opt_set(OPT_DUMP_ENTRY)) {
        DumpEntry(map, i, 1);
    }

    char buf[1024];
    fetchCanon(map, i, buf);
    char buf2[1024];
    Slashify(buf2, buf);
    return pfs_TranslateName(buf2);
}

/* Simplistic routine to convert old-style Cedar (IFS) file names to new-style. */
static void Slashify(char *to, char *from) {
    int lastC = 0;

    for (; *from != 0; ++from) {
	int c = *from;
	if (IsDelim(c)) {
	    c = '/';
	    if (lastC == '/') continue;
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

static char *cedarMapIFSName = "/Cedar/CedarVersionMap/CedarSource.VersionMap";
static struct Map *cedarMap = NULL;

void DumpStab() {
    if (cedarMap == NULL) cedarMap = ReadMap(cedarMapIFSName);
    for (int i = 0; i < cedarMap->len; i++) {
        uint32_t entry_index = cedarMap->locationIndex[i];
        char canon[1024];
        fetchCanon(cedarMap, entry_index, canon);

        int stab_index = cedarMap->entries[entry_index].index;
        printf("%d %d %s\n", entry_index, stab_index, canon);
    }
}

void DumpIndex() {
    if (cedarMap == NULL) cedarMap = ReadMap(cedarMapIFSName);
    for (int i = 0; i < cedarMap->len; i++) {
        uint32_t name_index = cedarMap->shortNames[i];
        printf("%d\n", name_index);
    }
}

void DumpAll(int swap) {
    if (cedarMap == NULL) cedarMap = ReadMap(cedarMapIFSName);
    for (int i = 0; i < cedarMap->len; i++) {
        DumpEntry(cedarMap, i, swap);
    }
}

void DumpSorted(int swap) {
    if (cedarMap == NULL) cedarMap = ReadMap(cedarMapIFSName);
    for (int i = 0; i < cedarMap->len; i++) {
        uint32_t stab = (swap) ? cedarMap->shortNames[i] : be32toh(cedarMap->shortNames[i]); // arg to Compare()
        DumpEntry(cedarMap, stab, swap);
    }
}

char *vermap_Translate(struct FSEntry *fe, char *name) {
    char *p = strrchr(name, '/');
    if (p == NULL) p = name; else ++p; // skip prefix

    // printf("vermap_Translate: %s\n", name);
    if (cedarMap == NULL) cedarMap = ReadMap(cedarMapIFSName);
    char *res = vermap_Lookup(cedarMap, p);
    return res;
}
