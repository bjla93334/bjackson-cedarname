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

// RAM is cheap!
// don't forget : free(buffer);
char *snarf(char *pathname, long *flen) {
    FILE *fp = fopen(pathname, "rb");
    if (fp == NULL) { perror("open"); return NULL; }

    fseek(fp, 0, SEEK_END);
    long file_length = ftell(fp);

    char *buffer = (char *)malloc(file_length);
    if (buffer == NULL) { perror("malloc"); fclose(fp); return NULL; }

    rewind(fp);
    size_t bytes_read = fread(buffer, 1, file_length, fp);
    if (bytes_read != file_length) { perror("read"); return NULL; }
    fclose(fp);

    if (flen != NULL) *flen = file_length;

    return buffer;
}

#include "pfs.h"
#include "options.h"

/*
    /cedar6.1/versionmap/.VersionMapImpl.mesa
    MyVersion: INT = 19850206; -- 012E.E3DE
    We got this number from the date February 6, 1985
*/

#define ShortKey	19850206
#define LongKey		19900710

/*
   These are binary data formats dictated by the Cedar runtime that generated the version map
   Currently, this is 32-bit Big Endian. Unfortunately, the file is not explicitly self-identifying.
 */
struct __attribute__((packed)) Stamp64 {
    uint16_t lo;
    uint16_t num;
    uint16_t hi;
    uint16_t extra;
};

struct __attribute__((packed)) Stamp48 {
    uint16_t lo;
    uint16_t num;
    uint16_t hi;
};

struct __attribute__((packed)) MapEntry128 {
    struct Stamp64 stamp;
    uint32_t created;		/* cedar time value */
    uint32_t index;		/* index of first char of long name */
};

struct __attribute__((packed)) MapEntry112 {
    struct Stamp48 stamp;
    uint32_t created;		/* cedar time value */
    uint32_t index;		/* index of first char of long name */
};

struct Map {
    long hstamp;                /* header stamp */
    long nEntries;			/* number of elements in each of next two  */
    uint32_t *shortNames;	/* index of an entry, in shortname order */
    struct MapEntry128 *entries;	/* the map entries */
    long nChars;		/* number of chars in names */
    char *names;		/* all the names */
    uint32_t *locationIndex;    /* 'btree' of location names (not from map) */
};

// be less clever about last entry
static int nameStart(struct Map *map, int i) {
    struct MapEntry128 *mep = &map->entries[i];
    return mep->index;
}

static int nameLast(struct Map *map, int i) {
    if (i == (map->nEntries - 1)) return map->nChars;
    struct MapEntry128 *mep = &map->entries[i + 1]; // follower
    return mep->index - 1;
}

// FIXME : max len?
// assert w/checking, or scan the stab and detect max length
static void fetchCanon(struct Map *map, int i, char *buf) {
    int start = nameStart(map, i);
    int end = nameLast(map, i);
    int nbytes = end - start;
    // assert(nbytes < 1024);
    strncpy(buf, map->names + start, nbytes);
    buf[nbytes] = 0;
}

// time_t epoch_time = 1784419200; (e.g., Tuesday, August 18, 2026)
static void format_utc(time_t epoch_time, char *buffer, size_t buflen) {
    struct tm utc_time;
    gmtime_r(&epoch_time, &utc_time);
    strftime(buffer, buflen, "%Y-%m-%d %H:%M:%S UTC", &utc_time);

    int debug = opt_set(OPT_DEBUG);
    if (debug) {
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
    }
}

static void DumpEntry(struct Map *map, int i) {
    uint32_t stab = map->shortNames[i]; // arg to CompareInPlace()

    struct MapEntry128 *mep = &map->entries[i];
    uint32_t index = mep->index; // index into string table

    struct Stamp64 stamp = mep->stamp;
    uint16_t lo = stamp.lo;
    uint16_t num = stamp.num;
    uint16_t hi = stamp.hi;
    uint16_t extra = stamp.extra;

    uint32_t created = mep->created;
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
static void FixMap128(struct Map *map) {
    for (int i = 0; i < map->nEntries; i++) {
        // pull out biggee-values;
        struct MapEntry128 *mep = &map->entries[i];
        uint32_t name_index = be32toh(map->shortNames[i]);

        uint32_t created = be32toh(mep->created);
        uint32_t index = be32toh(mep->index); // index into string table
        struct Stamp64 stamp = mep->stamp;
        uint16_t lo = be16toh(stamp.lo);
        uint16_t num = be16toh(stamp.num);
        uint16_t hi = be16toh(stamp.hi);
        uint16_t extra = be16toh(stamp.extra);

        // copy back host-order values;
        map->shortNames[i] = name_index;
        mep->created = created;
        mep->index = index;
        mep->stamp.lo = lo;
        mep->stamp.num = num;
        mep->stamp.hi = hi;
        mep->stamp.extra = extra;
    }
}

// insertion bubble-sort
void insertLocation(struct Map *map, int *table, int current_len) {
    int newbie_index = current_len;
    char newbie[1024];
    fetchCanon(map, newbie_index, newbie);
    // printf("%d %s\n", newbie_index, newbie);

    // FiXME : shouldn't assume max-len
    char opponent[1024];

    table[current_len] = newbie_index; // insert at 'bottom'
    for (int finger = current_len; finger > 0 ; finger--) {
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

/* File starts with three ASCII integers and CR. */
static int checkHeaderLine(char *body, long body_count) {
    long key, nEntries, nChars;
    int span;

    // 19900710, 7634, 326340, 20 13
    int count = sscanf(body, "%ld %ld %ld%n", &key, &nEntries, &nChars, &span);
    if (count != 3) return -1;
    if (key != LongKey) return -1;

    char ch = body[span];
    if (ch != '\r') return -1;

    int debug = opt_set(OPT_DEBUG);
    if (debug) { fprintf(stderr, "checkHeaderLine : %ld, %ld, %ld, %d, %d\n", key, nEntries, nChars, span, (int) ch); }

    // file length should (roughly) match the amount of data
    long total = 
        (nEntries * sizeof(struct MapEntry128))
        + (nEntries * sizeof(uint32_t))
        + nChars
    ;

    if (debug) {
        printf("names : %ld %ld %ld %ld %ld : %ld\n", 
            sizeof(struct MapEntry128), sizeof(uint32_t),
            nEntries, nChars,
            total, body_count
        );
    }

    if (total > body_count) return -1;
    return 0; // well-formed
}

static int checkHeaderBlock(char *body, long body_count) {
    uint16_t *header = (void *)body; // LOOPHOLE!
    int hstamp = be32toh((header[0] << 16) | header[1]);
    int nEntries = be32toh((header[2] << 16) | header[3]);

    // header: [dee3, 2e01] 19850206
    // header: [dc05, 0000] 1500
    int debug = opt_set(OPT_DEBUG);
    if (debug) { fprintf(stderr, "header: [%04x, %04x] %d\n", header[0], header[1], hstamp); }
    if (debug) { fprintf(stderr, "header: [%04x, %04x] %d\n", header[2], header[3], nEntries); }
    // map->hstamp = hstamp;
    // map->nChars = -1;
    if (hstamp == ShortKey) return 0;

    return -1;
}

/* Read the version map from disk. */
static struct Map *ReadMap(char *name) {
    long body_count = 0;
    char *body = snarf(name, &body_count);
    int v0 = checkHeaderBlock(body, body_count);
    int v1 = checkHeaderLine(body, body_count);
    if (v1 < 0) exit(0);

    FILE *fd = fopen(name, "r");
    if (fd == NULL) { perror(name); }
    if (fd == NULL) return NULL;

    /* File starts with three ASCII integers and CR. */
    long key, nEntries, nChars;
    int count = fscanf(fd, "%ld %ld %ld", &key, &nEntries, &nChars);
    if (count != 3) { fclose(fd); return NULL; }
    // fprintf(stderr, "%s : %s\n", name, "count : broken first line?");

    int c = getc(fd);
    if (c != '\r') { fclose(fd); return NULL; } // yes, CR, not LF
    // fprintf(stderr, "%s : %s\n", name, "endl : broken first line?");

    /* Only long format for now. */
    if (key != LongKey && key != ShortKey) { fclose(fd); return NULL; }
    // fprintf(stderr, "%s : %s(%ld)\n", name, "bad key", key);

    // another possible feature : dump version map stats:
    // printf("first line : %ld %ld %ld\n", key, nEntries, nChars);

    struct Map *map = (struct Map *) malloc(sizeof(*map));
    map->hstamp = key;
    map->nEntries = nEntries;

// FIXME : gonna remove this shortly
    map->entries = (struct MapEntry128 *) malloc((nEntries + 1) * sizeof(struct MapEntry128));
    map->shortNames = (uint32_t *) malloc(nEntries * sizeof(uint32_t));
    map->names = (char *) malloc(nChars);
    map->nChars = nChars;

    map->locationIndex = (int *) malloc(nEntries * sizeof(int));

    // printf("map : %ld %ld\n", map->nEntries, map->nChars);

    // table of MapEntry
    // table of shortNames
    // stab
    if (key == LongKey) {
        long xx = fread(map->entries, sizeof(struct MapEntry128), nEntries, fd);
        // printf("LongKey entries : %ld\n", xx);
        if (xx != nEntries) goto bad;

        long yy = fread(map->shortNames, sizeof(uint32_t), nEntries, fd);
        // printf("LongKey shortNames : %ld\n", yy);
        if (yy != nEntries) goto bad;

        FixMap128(map);
    }
    else {
        // FIXME: byte order? / dead code?
        unsigned short a[7];	/* for dealing with short form */
	for (int i = 0; i < nEntries; ++i) {
	    if (fread(a, sizeof(a), 1, fd) != 1) goto bad;
// FIXME	    nameStart(map, i) = ((long) a[6] << 16) | (long) a[5];
	    /* others : a[0-4] don't matter */
	}

	for (int i = 0; i < nEntries; ++i) {
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

// FIXME : remove this
// follower
    /*
      Make it easy to find the end of name.
      an extra entry was allocated just for this
     */
    map->entries[nEntries].index = nChars;
    fclose(fd);

    // build locationIndex, ordered appropriately
    for (int i = 0; i < map->nEntries; i++) {
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

static int CompareInPlace(struct Map *map, char *name, int index);

static int CompareStamp64(struct Map *map, int version, int index) {
    struct MapEntry128 *mep = &map->entries[index];
    struct Stamp64 stamp = mep->stamp;
    int entry_stamp = ((int) stamp.num << 16) + stamp.hi;
    int distance = version - entry_stamp;

    // printf("CompareStamp64 %d %04x %04x %d\n", index, version, entry_stamp, distance);
    return distance;
}

/* binary search of entry list */
static int FindStamp64(struct Map *map, int version) {
    int lo = 0;
    int hi = map->nEntries - 1;

    while (lo <= hi) {
	int index = (lo + hi) / 2;

        // printf("FindStamp64 %d %d %d\n", lo, hi, index);
	int r = CompareStamp64(map, version, index);
	if (r < 0) {
	    if (lo == index) break;
	    hi = index - 1;
	}
	else if (r > 0) {
	    if (hi == index) break;
	    lo = index + 1;
	}
	else /* equal */
	    return index -1;
    }

    return -1;
}

/* Do the binary search in the tree. */
static int ShortNameFind(struct Map *map, char *name) {
    int lo = 0;
    int hi = map->nEntries - 1;

    while (lo <= hi) {
	int index = (lo + hi) / 2;

        // printf("ShortNameFind %d %d %d\n", lo, hi, index);
	int r = CompareInPlace(map, name, map->shortNames[index]);
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

// FIXME:
static int CompareInPlace(struct Map *map, char *name, int index) {
    struct MapEntry128 *mep = &map->entries[index];
    int start = mep->index;
    int end = map->nChars; // ugly control flow!
    if (index != (map->nEntries - 1)) {
        struct MapEntry128 *follower = &map->entries[index + 1];
        end = follower->index - 1;
    }

    // printf("CompareInPlace %d %d %d\n", start, end, 0);

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

    // printf("CompareInPlace2 %d %d %d\n", start, end, end - start);
    int xlen = end - start;

    return strncasecmp(name, map->names + start, end - start);
}

static void Slashify(char *to, char *from);

static char *vermap_Lookup(struct Map *map, char *name) {
    if (map == NULL) { pfs_errorMsg = "Can't find, no version map."; return NULL; }

    int i = ShortNameFind(map, name);
    if (i == -1) { pfs_errorMsg = "Can't find in version map."; return NULL; }

    /* Found it. */
    if (opt_set(OPT_DUMP_ENTRY)) {
        DumpEntry(map, i);
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

char *localFSName;
static struct Map *cedarMap = NULL;

void setMapName(char *name) {
    localFSName = name;
}

void DumpStab() {
    if (cedarMap == NULL) cedarMap = ReadMap(localFSName);
    for (int i = 0; i < cedarMap->nEntries; i++) {
        uint32_t entry_index = cedarMap->locationIndex[i];
        struct MapEntry128 *mep = &cedarMap->entries[entry_index];
        int stab_index = mep->index;

        char canon[1024];
        fetchCanon(cedarMap, entry_index, canon);
        printf("%d %d %s\n", entry_index, stab_index, canon);
    }
}

void DumpIndex() {
    if (cedarMap == NULL) cedarMap = ReadMap(localFSName);
    for (int i = 0; i < cedarMap->nEntries; i++) {
        uint32_t name_index = cedarMap->shortNames[i];
        printf("%d\n", name_index);
    }
}

void DumpAll() {
    if (cedarMap == NULL) cedarMap = ReadMap(localFSName);
    for (int i = 0; i < cedarMap->nEntries; i++) {
        DumpEntry(cedarMap, i);
    }
}

void DumpSorted() {
    if (cedarMap == NULL) cedarMap = ReadMap(localFSName);
    for (int i = 0; i < cedarMap->nEntries; i++) {
        uint32_t stab = cedarMap->shortNames[i]; // arg to CompareInPlace()
        DumpEntry(cedarMap, stab);
    }
}

char *vermap_Translate(struct FSEntry *fe, char *name) {
    char *p = strrchr(name, '/');
    if (p == NULL) p = name; else ++p; // skip prefix

    // printf("vermap_Translate: %s\n", name);
    if (cedarMap == NULL) cedarMap = ReadMap(localFSName);
    char *res = vermap_Lookup(cedarMap, p);
    return res;
}

char *vermap_LookupStamp64(int stamp) {
    // printf("vermap_Translate: %s\n", name);
    if (cedarMap == NULL) cedarMap = ReadMap(localFSName);
    int stamp_index = FindStamp64(cedarMap, stamp);

// debug
    DumpEntry(cedarMap, stamp_index);

    char canon[1024];
    fetchCanon(cedarMap, stamp_index, canon);
    char *p = malloc(strlen(canon) + 1);
    strcpy(p, canon);
    return p;

    /*
    char buf2[1024];
    Slashify(buf2, canon);
    return pfs_TranslateName(buf2);
    */
}
