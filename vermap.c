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

struct __attribute__((packed)) MapEntry128 {
    struct Stamp64 stamp;
    uint32_t created;		/* cedar time value */
    uint32_t index;		/* index of first char of long name */
};

struct __attribute__((packed)) Stamp48 {
    uint16_t lo;
    uint16_t num;
    uint16_t hi;
};

struct __attribute__((packed)) MapEntry112 {
    struct Stamp48 stamp;
    uint16_t created[2];        /* cedar time value */
    uint16_t index[2];		/* index of first char of long name */
};

struct Map {
    long hstamp;                /* header stamp */
    long nEntries;		/* number of elements in each of next two  */
    long nChars;		/* number of chars in names */
    uint32_t *shortNames;	/* index of an entry, in shortname order */
    struct MapEntry128 *entries; /* the map entries */
    char *names;		/* all the names */
    int *locationIndex;         /* 'btree' of location names (not from map) */
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

static void DumpEntry(struct Map *map, int i); // ugh, recursive!

// FIXME : max len?
// assert w/checking, or scan the stab and detect max length
static void fetchCanon(struct Map *map, int i, char *buf) {
    int start = nameStart(map, i);
    int end = nameLast(map, i);
    int nbytes = end - start;

    int debug = opt_set(OPT_DEBUG);
    // if (debug) fprintf(stderr, "fetch: %d..%d %d\n", start, end, nbytes);

    int rope_len = map->nChars;

    if (start >= rope_len) {
        fprintf(stderr, "out-of-bounds : %s, %d\n", "start", start);
        goto bad;
    }

    if (end > rope_len) {
        fprintf(stderr, "out-of-bounds : %s, %d\n", "end", end);
        goto bad;
    }

    if (nbytes > 100) {
        fprintf(stderr, "out-of-bounds : %s, %d\n", "nbytes", nbytes);
        goto bad;
    }

    // assert(nbytes < 1024);
    strncpy(buf, map->names + start, nbytes);
    buf[nbytes] = 0;
    return;
bad:
    strncpy(buf, "bogus", 6);
    // DumpEntry(map, i);
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
        fprintf(stderr, "%04d-%02d-%02d %02d:%02d:%02d UTC\n",
           utc_time.tm_year + 1900,
           utc_time.tm_mon + 1,
           utc_time.tm_mday,
           utc_time.tm_hour,
           utc_time.tm_min,
           utc_time.tm_sec
        );
    }
}

/*
   'created' is a BasicTime - seconds since 1901 / 1968 ; so wrong unix epoch
   Alto: 1901 to 2036
   XNS: 1968 to 2103 (Alto offset to 1968)

    1991-05-13 14:05:58.000000000 -0700 /r/Tioga.tip
   -2208988800 UTC: Monday, January 1, 1900 at 12:00:00 AM
   -63158400 UTC: Monday, January 1, 1968 at 12:00:00 AM
   0 UTC: Thursday, January 1, 1970 at 12:00:00 AM
*/
static void DumpEntry(struct Map *map, int i) {
    uint32_t stab = map->shortNames[i]; // arg to CompareInPlace()

    struct MapEntry128 *mep = &map->entries[i];
    uint32_t index = mep->index; // index into string table

    struct Stamp64 stamp = mep->stamp;
    uint16_t lo = stamp.lo;
    uint16_t num = stamp.num;
    uint16_t hi = stamp.hi;
    uint16_t extra = stamp.extra;

    int entry_stamp = ((int) stamp.num << 16) + stamp.hi;

    uint32_t created = mep->created;
    int fudge = (-24*60*60) + (-2*365*24*60*60); // leap year ??
    uint64_t utc = fudge + (uint64_t) created;
    char buffer[80];
    format_utc(utc, buffer, sizeof(buffer));
    char *calendar = buffer;

    char canon[1024];
    fetchCanon(map, i, canon);

    printf("Entry #%d %08x %s %s (%d, %ld)\n", i, entry_stamp, canon, calendar, created, utc);

    int debug = opt_set(OPT_DEBUG);
    // FIXME : perhaps use JSON and have this be a feature ?
    if (debug) fprintf(stderr,
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
        uint32_t name_index = be32toh(map->shortNames[i]);

        struct MapEntry128 *mep = &map->entries[i];
        struct Stamp64 stamp = mep->stamp;
        uint16_t lo = be16toh(stamp.lo);
        uint16_t num = be16toh(stamp.num);
        uint16_t hi = be16toh(stamp.hi);
        uint16_t extra = be16toh(stamp.extra);
        uint32_t created = be32toh(mep->created);
        uint32_t index = be32toh(mep->index); // index into string table

        // copy back host-order values;
        map->shortNames[i] = name_index;
        mep->stamp.lo = lo;
        mep->stamp.num = num;
        mep->stamp.hi = hi;
        mep->stamp.extra = extra;
        mep->created = created;
        mep->index = index;
    }
}

// insertion bubble-sort
void insertLocation(struct Map *map, int *table, int current_len) {
    int newbie_index = current_len;
    char newbie[1024];
    fetchCanon(map, newbie_index, newbie);
    // fprintf(stderr, "%d %s\n", newbie_index, newbie);

    // FiXME : shouldn't assume max-len
    char opponent[1024];

    table[current_len] = newbie_index; // insert at 'bottom'
    for (int finger = current_len; finger > 0 ; finger--) {
        int opponent_index = table[finger-1];
        fetchCanon(map, opponent_index, opponent);
        int placing = strcmp(newbie, opponent); // who cares about matches ??
        // fprintf(stderr, "%d %s %d\n", newbie_index, newbie, placing);

        // if (newbie > opponent) break;
        if (placing > 0) break;

        // swap these
        table[finger] = opponent_index;
        table[finger-1] = newbie_index;
    }
}

/* File starts with three ASCII integers and CR. */
static int checkHeaderLine(char *body, long body_count, struct Map *map) {
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
        fprintf(stderr, "names : %ld %ld %ld %ld %ld : %ld\n", 
            sizeof(struct MapEntry128), sizeof(uint32_t),
            nEntries, nChars,
            total, body_count
        );
    }

    if (total > body_count) return -1;

    map->hstamp = key;
    map->nEntries = nEntries;
    map->nChars = nChars;
    return 0; // well-formed
}

static int checkHeaderBlock(char *body, long body_count, struct Map *map) {
    uint16_t *header = (void *)body; // LOOPHOLE!
    int hstamp = be32toh((header[0] << 16) | header[1]);
    int nEntries = be32toh((header[2] << 16) | header[3]);

    int debug = opt_set(OPT_DEBUG);
    if (debug) fprintf(stderr, "body_count %ld\n", body_count);

    // header: [dee3, 2e01] 19850206
    // header: [dc05, 0000] 1500
    if (debug) { fprintf(stderr, "header: [%04x, %04x] %d\n", header[0], header[1], hstamp); }
    if (debug) { fprintf(stderr, "header: [%04x, %04x] %d\n", header[2], header[3], nEntries); }
    if (hstamp != ShortKey) return -1;

// FIXME : remove this junk
    int dead = 0;
    if (dead)  {
        // od -t x2 data/CedarSource.VersionMap\!34 | grep 2ba5
        // 0056700 8900 3402 6402 2ba5 0000 435b 6465 7261
        // 2ba5 0000 - 42283

        int offset = 0056700; // from od, approx
        int nChars = 42283;
        // map->hstamp = hstamp;
        // map->nChars = nChars;
        fprintf(stderr, "0x%08x %d nChars %d @ %d\n", hstamp, hstamp, nChars, offset);

        // 0x012ee3de 19850206 nChars 42283 24000
        // bulk 24013, grain 16 bits 128
        int bulk = body_count - nChars;
        int grain = bulk / nEntries;
        fprintf(stderr, "bulk %d, grain %d bits %d\n", bulk, grain, grain * 8);
    }

    map->hstamp = hstamp;
    map->nEntries = nEntries;
    map->nChars = -1;
    return 0; // well-formed
}

// all this to just skip 8 bytes!
static int TryRawHeader(FILE *fd, struct Map *map) {
    rewind(fd);

    uint16_t header[3];
    long yy = fread(header, sizeof(header), 1, fd);

    int debug = opt_set(OPT_DEBUG);
    if (debug) { fprintf(stderr, "yy: %ld, %ld\n", yy, sizeof(header)); }
    if (yy != 1) return 0;

    for (int i = 0; i < 3; i++) { header[i] = be16toh(header[i]); }

    int hstamp = (header[1] << 16) | header[0];
    int nEntries = header[2];
    if (debug) { fprintf(stderr, "header: [%04x, %04x] %d\n", header[1], header[0], hstamp); }
    if (debug) { fprintf(stderr, "header: [%04x] %d\n", header[2], nEntries); }
    // header: [012e, e3de] 19850206
    // header: [05dc] 1500

    // map->hstamp = hstamp;
    // map->nEntries = nEntries;
    // map->nChars = -1;

    if (hstamp != ShortKey) return -1;

    return 0;
}

// [Cedar]<Cedar6.1>
// PCRuntime>LupineRuntime.mesa!1
static void DumpRope(char *p, int len) {
    printf("\nrope:\n");
    for (int i = 0; i < len; i++) {
        char ch = p[i];
        if (ch == '\r') ch = '|';
        putc(ch, stdout);
        if ((i % 40) == 39) printf("\n");
    }
    printf("\n");
}

/* Read the version map from disk. */
static struct Map *ReadMap(char *name) {
    long body_count = 0;
    char *body = snarf(name, &body_count);

    struct Map *map = (struct Map *) malloc(sizeof(*map));

    int v0 = checkHeaderBlock(body, body_count, map);
    int v1 = checkHeaderLine(body, body_count, map);
    if ((v0 < 0) && (v1 < 0)) goto bad;

    // in theory, only one of the checkHeader method sets these!
    int key = map->hstamp;
    int nEntries = map->nEntries;

    int debug = opt_set(OPT_DEBUG);
    if (debug) fprintf(stderr, "key %d nEntries %d\n", key, nEntries);

    FILE *fd = fopen(name, "r");
    if (fd == NULL) { perror(name); }
    if (fd == NULL) return NULL;

int old_trash = 0;
if (old_trash) {
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
    // fprintf(stderr, "first line : %ld %ld %ld\n", key, nEntries, nChars);
}

// FIXME : gonna remove this shortly
    map->entries = (struct MapEntry128 *) malloc((nEntries + 1) * sizeof(struct MapEntry128));
    map->shortNames = (uint32_t *) malloc(nEntries * sizeof(uint32_t));
    map->locationIndex = (int *) malloc(nEntries * sizeof(int));

    // fprintf(stderr, "map : %ld %ld\n", map->nEntries, map->nChars);

    // table of MapEntry
    // table of shortNames
    // stab
    if (key == LongKey) {
        // just to skip the first line, ugh!
        {
            long key, nEntries, nChars;
            int count = fscanf(fd, "%ld %ld %ld", &key, &nEntries, &nChars);
            int c = getc(fd);
        }

        long xx = fread(map->entries, sizeof(struct MapEntry128), nEntries, fd);
        // fprintf(stderr, "LongKey entries : %ld\n", xx);
        if (xx != nEntries) goto bad;

        long yy = fread(map->shortNames, sizeof(uint32_t), nEntries, fd);
        // fprintf(stderr, "LongKey shortNames : %ld\n", yy);
        if (yy != nEntries) goto bad;

        FixMap128(map); // all at once
    }
    else {
        // start again
        rewind(fd);
        int bb = TryRawHeader(fd, map);
        if (bb < 0) goto bad;

        // long nEntries =  map->nEntries;

        long pos_map = ftell(fd);

        /* others : a[0-4] don't matter */
        // au contraire mon-ami!
        struct MapEntry112 tiny; // 7 words, 14 bytes
	for (int i = 0; i < nEntries; i++) {
	    if (fread(&tiny, sizeof(tiny), 1, fd) != 1) goto bad;
            // deal with endian-ness, one entry at a time
            uint16_t lo = be16toh(tiny.stamp.lo);
            uint16_t num = be16toh(tiny.stamp.num);
            uint16_t hi = be16toh(tiny.stamp.hi);
            uint16_t c0 = be16toh(tiny.created[0]);
            uint16_t c1 = be16toh(tiny.created[1]);
            uint16_t si0 = be16toh(tiny.index[0]);
            uint16_t si1 = be16toh(tiny.index[1]);
            uint32_t created = (c1 << 16) | c0;
            uint32_t index = (si1 << 16) | si0;

            struct MapEntry128 *mep = &map->entries[i]; //  nameStart(map, i) = loc_index;
            mep->stamp.lo = lo;     // a[0]
            mep->stamp.num = num;   // a[1]
            mep->stamp.hi = hi;     // a[2]
            mep->created = created; // a[3..4] /* cedar time value */
            mep->index = index;     // a[5..6] /* index into string table (long name) */
	}

        long pos_names = ftell(fd);

        // could block read [uint16_t x nEntries] here
        // but then we'd need a 2nd copy (buffer)
	for (int i = 0; i < nEntries; i++) {
            // could read a uint16_t here and then be16toh that value
            // int16, 2 bytes
	    int c1 = getc(fd);
	    int c2 = getc(fd);
	    if (c1 == EOF || c2 == EOF) goto bad;
            int name_index = (c1 << 8) | c2;
	    map->shortNames[i] = name_index; // index into string table (short name)
	}

        long pos_nchar = ftell(fd);

        // now get nChars!
        uint16_t thing;
        if (fread(&thing, sizeof(thing), 1, fd) != 1) goto bad;

        int debug = opt_set(OPT_DEBUG);
        if (debug) fprintf(stderr, "pos %ld %ld %ld\n", pos_map, pos_names, pos_nchar);

        int rope_len = be16toh(thing);
        if (debug) fprintf(stderr, "rope"
            " sz %ld"
            " %04x thing %d"
            " len %d"
            "\n",
            sizeof(thing),
            thing, thing,
            rope_len
        );

        map->nChars = rope_len;
        // map->nChars = 42283;
    }

    long pos_rope = ftell(fd);

    int nChars = map->nChars;

    // read stab 23363
    if (debug) { fprintf(stderr, "read stab @ %ld %d\n", pos_rope, nChars); }
    if (nChars <= 0) goto bad;

    map->names = (char *) malloc(nChars);
    long zz = fread(map->names, 1, nChars, fd);
    long pos_end = ftell(fd);

    if (debug) fprintf(stderr, "stab pos %ld %ld %ld\n", pos_rope, pos_end, body_count);

    // bytes read 23363
    if (debug) fprintf(stderr, "bytes read %ld\n", zz);
    if (zz != nChars) {
        goto bad;
    }

    if (debug) DumpRope(map->names, 200);

// FIXME : remove this
// follower
    /*
      Make it easy to find the end of name.
      an extra entry was allocated just for this
     */
    map->entries[nEntries].index = nChars;
    fclose(fd);

    // build locationIndex, ordered appropriately
    for (int i = 0; i < nEntries; i++) {
        insertLocation(map, map->locationIndex, i);
    }
    return map;
bad:
    if (body) free(body);
    if (map) {
        if (map->names) free(map->names);
        if (map->shortNames) free(map->shortNames);
        if (map->entries) free(map->entries);
        if (map->locationIndex) free(map->locationIndex);
        free(map);
    }
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

    // fprintf(stderr, "CompareStamp64 %d %04x %04x %d\n", index, version, entry_stamp, distance);
    return distance;
}

/* binary search of entry list */
static int FindStamp64(struct Map *map, int version) {
    int lo = 0;
    int hi = map->nEntries - 1;

    while (lo <= hi) {
	int index = (lo + hi) / 2;

        // fprintf(stderr, "FindStamp64 %d %d %d\n", lo, hi, index);
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

        // fprintf(stderr, "ShortNameFind %d %d %d\n", lo, hi, index);
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

    // fprintf(stderr, "CompareInPlace %d %d %d\n", start, end, 0);

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

    // fprintf(stderr, "CompareInPlace2 %d %d %d\n", start, end, end - start);
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
        int entry_index = cedarMap->locationIndex[i];
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

    // fprintf(stderr, "vermap_Translate: %s\n", name);
    if (cedarMap == NULL) cedarMap = ReadMap(localFSName);
    char *res = vermap_Lookup(cedarMap, p);
    return res;
}

char *vermap_LookupStamp64(int stamp) {
    // fprintf(stderr, "vermap_Translate: %s\n", name);
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
