# bjackson-cedarname

project to update the 'cedarname' utility originally from :

    CSL-93-16/cedarname

The hosting environment is Ubuntu 26.04 installed on a NUC8i3BEK,
with qemu-system-sparc running a virtualized Cedar desktop system.

Direct access to items from the CSL-93-16.iso is a handy way to get started.

# XeroxCedar

The environment variable ${XeroxCedar} is used to locate the Cedar10.1 release.
For example:

    setenv XeroxCedar ~/Desktop/CSL-93-16/Cedar

# Q : What is a VersionMap?

The VersionMap for a "Cedar Release" is a (pair of) data structures designed to reduce the cost of searching for items (i.e. files) that constitute the release.  The lookup functions supported are "by (short) name" and "by version stamp".

Let's back up a bit.

A Cedar Release is simply a (closed) set of files.
Each file object has 3 attributes :
    a 'location',
    a "create date" (aka mtime), and
    a (48-bit) "version stamp".

# Location(s)

A location (aka pathname) is a bit complicated, informally think of it as being an IFS name of the form :

    [server]<directory>subdirs>shortname!version

The shortname is further structured as a (base, ext) pair separted by a dot (.) where base doesn't contain any dots, but ext may (ugh/yuck).  For the purposes of 'cedarname' we only have to worry about the shortname.

# Representing Time

As you might expect - everyone, even Cedar, has their own notion of time.  Thankfully, Cedar is similar to unix and instances of time have integer values, representing a (unique) distance / interval from a fixed 'epoch' value (i.e. 0).
Sadly, Cedar's BasicTime epoch differs from the de-facto unix value, so an adjustment value is added to compensate so that we can use unix/posix utilities to express the numeric value as a "calendar string" which users prefer (!!) when trying to understand when instances of time happened.

# Version Stamps

Version Stamps are GUID's, which are constructed (pseudo) 'randomly'.
There seems to be some fuzzyness around how various Cedar releases implemented GUIDs,
for the present release (Cedar 10.1), 64 bits are available in the data, but only 32 bits (num and hi) are non-zero.

# VersionMap data

Ignoring for the moment byte-order issues (network vs. host, big vs little),
The VersionMap file (data) has 3 sections :
the shortname 'index',
the file entry list,
and a compact character table (stab).i
Oh, and the first "line" which indicates how many entries there are.

The file entry list contains the tuples, with the location string represented as a numeric reference into the stab so that the records are fixed length.
There's a trick being done here in 'cedarname' with the character table where the variable length strings are a sequential log, and there's an extra (null) file entry added when reading the data file that supplies the "following index" for the last location string.
Location string length is computed by using the 'start' from the file entry and it's "follower" start entry.
This means you don't have to check how long the character table is - that's done once when the file is read into memory.

NOTE : I'm about to change that!

The file entry list is kept in 'stamp' order, and
the shortname list is kept in alphabetical order.

The 2 maps can therefore be quickly accessed using a binary search algorithm.

# Endian-ness philosophy

Oh, so I admit it.  I was lazy.  The original task was just to get cedarname working
well enough so that I could use it on my linux host and avoid running within a SPARC32 qemu enironment.

Now my ambitions have grown and I'd like to be able use it with 'other' maps.
In particular, I'd like to leverage the Alto IFS archives for older Cedar releases.
To do that I've gotta generalize the code more, and I'd really rather not invest tons of effort right now
to sort out all the VersionMap formats that ever existed.
I'm gonna pick 'em off one at a time (maybe).

I also don't really want to use a 'serdes' mindset when reading in the binary file(s).

# A different way of looking at things

In stead of fighting to get things sorted out while doing I/O,
I'm using a long standing approach of mine - just inhale the whole file,
and then deal with things.

# Two options : in-memory data conversion w/copy, or 'getter' endian conversion.

I haven't made up my mind yet but ...  when I consider choosing, I know I'm going to have
to write per-version conversion functions.  That might favor data copy.
I'm tempted to be "object oriented" here an group conversions into an accessor object (or a conversion object).
Still mulling things over ...

# Original Definitive Reference for VersionMap and DF's - CSL-82-7, pg 85, Fig 4.3

    I highly recommend reading Eric's thesis.

    Eric Schmidt's thesis
    Xerox PARC Technical Report, CSL-82-7, December 1982
    http://www.bitsavers.org/pdf/xerox/parc/techReports/CSL-82-7_Controlling_Large_Software_Development_In_a_Distributed_Environment.pdf

    https://www2.eecs.berkeley.edu/Pubs/TechRpts/1982/7596.html

    @phdthesis{Schmidt:7596,
        Author= {Schmidt, Eric E.},
        Title= {Controlling Large Software Development in a Distributed Environment},
        School= {EECS Department, University of California, Berkeley},
        Year= {1982},
    }

    I was unaware of it until just recently (Aug 2026).

    I'm down this Rabbit-Hole getting perspective on how this early work relates to the de-facto use of DF's and the evolutionary path(s) our tooling traveled. My experience is/was clearly very different from the research inrtent of the "System Modeller", as least as things go relative to the editor informing the SM of updates and immediately compiling modules and doing system updates (live module replacement).

    What I can say is that there's a high degree of fidelity between the Early Cedar work, the last Dorado Cedar release, and the early days of Mimosa / PCedar.

    I should probably mention something about MakeDo at this point, but let me hold off on that until I wrap my headed around things a bit more

# build philosophy

I'm a minimalist.  The upside for that is when tools evolve things don't break
provided that the tool culture is relatively stable.  The downside is that I'm
relying upon assumtions, which can make things fragile.

I'm going for a implicitly defined Makefile, take a look and you'll see what I mean.

# PCR build philosophy

In contrast to being minimalist, the PCR build makes an attempt to provide for
many possible hosting situations.  That made a great deal of sense circa 1990
as technology was evolving quickly and there were lots of hardware and software
candidates vying for frontrunning or 'winner' status.

Clearly, 32-bit SPARC and SunOS 4.1.4 have been dead for a while.  Today (2026),
x86_64 is the clear cpu winner (although there's movement in the direction of ARM).

# Quick test(s)

There's two very obvious things folks will be interested in,
the location lookup (original functionality), and then for the
more curious minded, the internal details, so let's use them for testing :

    ./cedarname /r/PFS.mesa
    ./cedarname /r/Rope.mesa

    ./cedarname --dumpEntry /r/Tioga.tip
    ./cedarname --dumpAll
    ./cedarname --dumpIndex
    ./cedarname --dumpSorted
    ./cedarname --dumpPrefixMap
    ./cedarname --dumpStab

# Lookup by Stamp

Now that I've read Eric's thesis, and imagined what
things were like in the "olden days", I've gone ahead
and begun to implement :

    vermap_LookupStamp64

I just tossed this together w/o deep thinking.
It seems to work, but I don't know where the offbyon[e] (x-1),
thingy came from ??

# Cedar size definitions, check IFS archives

    /cyan/cedar6.1/mesaruntime/.Basics.mesa!1.html
    ./cedarname /r/Basics.mesa

# ugh, other map file format(s)

    ./cedarname --debug --mapFile data/CedarSource.VersionMap\!34  /r/Rope.mesa

# Bah! - data/CedarSource.VersionMap

I've got some silly bug hiding from me,
use the big hammer (od) to bench check this.

0000000 2e01dee3 0000dc05 449e2200 3e20a26d
0000020 00001200 49000000 c96d6a9e 32006420
0000040 00000000 b19e9500 ab20156e 00004a00
0000060 af000000 2f6e879e 64008120 00000000

0000000 dee3 2e01 dc05 0000 2200 449e a26d 3e20
0000020 1200 0000 0000 4900 6a9e c96d 6420 3200
0000040 0000 0000 9500 b19e 156e ab20 4a00 0000
0000060 0000 af00 879e 2f6e 8120 6400 0000 0000

0000000 e3 de 01 2e 05 dc 00 00 00 22 9e 44 6d a2 20 3e
0000020 00 12 00 00 00 00 00 49 9e 6a 6d c9 20 64 00 32
0000040 00 00 00 00 00 95 9e b1 6e 15 20 ab 00 4a 00 00
0000060 00 00 00 af 9e 87 6e 2f 20 81 00 64 00 00 00 00

# debug info

version:  header: [dee3, 2e01] 19850206 (February 6, 1985)
nEntries: header: [dc05, 0000] 1500

map.entries,
map.shortNameSeq,
namesChars

checking file positions:

    pos 8 21008 24008 24012 47375 66296

# header and first 3 map entries :

0000000
e3 de 01 2e 05 dc 00 00 / dee3 2e01 dc05 0000 / 2e01dee3 0000dc05 - header

0000010
00 22 9e 44 6d a2 20 3e / 2200 449e a26d 3e20 / 449e2200 3e20a26d - entry[0]
00 12 00 00 00 00 00 49 / 1200 0000 0000 4900 / 00001200 49000000
9e 6a 6d c9 20 64 00 32 / 6a9e c96d 6420 3200 / c96d6a9e 32006420
00 00 00 00 00 95 9e b1 / 0000 0000 9500 b19e / 00000000 b19e9500
6e 15 20 ab 00 4a 00 00 / 156e ab20 4a00 0000 / ab20156e 00004a00
00 00 00 af 9e 87 6e 2f / 0000 af00 879e 2f6e / af000000 2f6e879e
20 81 00 64 00 00 00 00 / 8120 6400 0000 0000 / 64008120 00000000

stamp:   [2200 449e a26d]
created: [3e20 1200]
index:   [0000 0000]

stamp:   [4900 6a9e c96d]
created: [6420 3200]
index:   [0000 0000]

stamp:   [9500 b19e 156e]
created: [ab20 4a00]
index:   [0000 ]

# names

from start : 8 + (1500 * 14) : 21008
from end   : 24008 - (1500 * 2) : 21008

    --skip-bytes 21016

od --address-radix d --skip-bytes 21008 -t x2 data/CedarSource.VersionMap!34 | head

cf04 8200 2200 5501 0204 0703 4802 cd02
8d01 0302 b500 6d02 9903 4805 1e02 2404
1902 1f02 7003 7203 f000 2802 ac00 bd00

# Stab
pos 8 21008 24008
rope sz 4 5b43 lo 23363 0000 hi 0 len 23363
read stab @ 24012 23363
stab pos 24012 47375 66296


    --skip-bytes 24006

    nChars [2ba5 0000] : 42283
    [Cedar]<Cedar6.1>[Cedar]<Cedar6.1>
    PCRuntime>LupineRuntime.mesa!1
    ...
    MakeBoot>MakeMakeBoot.cm!1

od --address-radix d --skip-bytes 24000 -t x2 -c data/CedarSource.VersionMap!34 | head
0024000    8900    3402    6402    2ba5    0000    435b    6465    7261
         \0 211 002   4 002   d 245   +  \0  \0   [   C   e   d   a   r
0024016    3c5d    6543    6164    3672    312e    0d3e    5052    5243
          ]   <   C   e   d   a   r   6   .   1   >  \r   R   P   C   R
0024032    6e75    6974    656d    4c3e    7075    6e69    5265    6e75
          u   n   t   i   m   e   >   L   u   p   i   n   e   R   u   n
0024048    6974    656d    6d2e    7365    2161    0d31    6954    676f
          t   i   m   e   .   m   e   s   a   !   1  \r   T   i   o   g

# Cedar6.1 Implementation

First, Kudo's to Russ Atkinson ; I love reading his code.
If people like my code style, bonus points for Russ.

/cyan/cedar6.1/versionmap/VersionMapImpl.mesa!1

MyVersion: INT = 19850206; -- 012E.E3DE
    -- Every saved version stamp file needs this number at its start.
    We got this number from the date February 6, 1985, and
    we suggest that future versions also use this convention for generating this number.

MyStamp: TYPE = MACHINE DEPENDENT RECORD [lo,num,hi: CARDINAL];
SaveMapToFile: PUBLIC PROC [map: Map, name: ROPE] = TRUSTED {
    st: IO.STREAM ← FS.StreamOpen[name, $create];
    names: ROPE ← map.names;

    myVersion: INT ← MyVersion;
    len: NAT ← map.len;
    namesChars: INT ← names.Size[];

    IO.UnsafePutBlock[st, [LOOPHOLE[LONG[@myVersion]], 0, SIZE[INT]*bytesPerWord]];
        -- first, output the internal version number
        -- 4 bytes, INT32
    IO.UnsafePutBlock[st, [LOOPHOLE[LONG[@len]], 0, SIZE[NAT]*bytesPerWord]];
        -- first, output the # of entries
        -- 4 bytes, NAT31
    IO.UnsafePutBlock[st, [LOOPHOLE[@map.entries[0]], 0, len*(SIZE[MapEntry]*bytesPerWord)]];
        -- next, the entries themselves
        -- MapEntry112
    IO.UnsafePutBlock[st, [LOOPHOLE[@map.shortNameSeq[0]], 0, len*(SIZE[CARDINAL]*bytesPerWord)]];
        -- next, the shortNameSeq
        -- CARD16[len]
    IO.UnsafePutBlock[st, [LOOPHOLE[LONG[@namesChars]], 0, (SIZE[INT]*bytesPerWord)]];
        -- next, the number of characters we are about to write (not counting the end marker)
        -- 4 bytes, INT32
    IO.PutRope[st, names];
        -- then the names rope
        -- CHAR8[namesChars]
    IO.PutRope[st, "\000\000\000"];
        -- marker at end to make Tioga happy
        -- 3 bytes
    IO.SetLength[st, IO.GetIndex[st]]; -- force goddamm truncation already!!!
    IO.Close[st];
};

    body_count 66296
    header: [dee3, 2e01] 19850206
    header: [dc05, 0000] 1500
    0x012ee3de 19850206 nChars 42283 24000
    bulk 24013, grain 16 bits 128

    13 = 3 x NUL, header(8), nChars(2)


# Implementor Notes - Archaeology or Forensics ??

    The 'stab' (Index) is NOT paired with the Entry in dumpAll.
    Don't think too hard about it - just think slice 'i' of independent tables.

    ./cedarname --dumpSorted | egrep 'canon|utc' | less
    ./cedarname --dumpAll | egrep 'canon|stamp' | sed -e 's| 0000 lo:.*||' | less
    ./cedarname --dumpAll | egrep ' i:|stab:' > /tmp/stab-order
    ./cedarname --dumpStab | sort -n -k2 | less

    Hmmm, stab ordering isn't what I expected ??
    Symbol Table starts with : "[Cedar10.1]<Top>", and does end at MMMKeyboard.mesa!2
    there's CR's between values and it's one big Rope (not ansi-C strings with NUL chars)

    0 17 [Cedar10.1]<Phoenix>PhSwitchImpl.mesa!1
    1 57 [Cedar10.1]<Commands>SlateSessions.command!1
    ...
    7632 326257 [Cedar10.1]<ColorTrix>ColorTrixViewer.require!1
    7633 326305 [Cedar10.1]<MMM>MMMKeyboard.mesa!2

    ./cedarname --dumpEntry /r/CedarSource.VersionMap

    canon: [Cedar10.1]<CedarVersionMap>CedarSource.VersionMap!63
    305e03b3 created: 811467699
    2c9a4b33 utc: 748309299 (1993-09-17 23:41:39 UTC)

# DF Syntax

    A DF file is essentially a bill-of-materials and dependency graph tracker.
    It specifies file names, fully qualified network locations (IFS/Grapevine servers),
    and specific creation timestamps to guarantee environment consistency.

    Over time, usage has evolved and while the syntax may seem to be human-oriented,
    in actuality there are 2 distinct cases : tool generated and fuzzy tool-input "templates".

    In order to explore the tool output form, I've roughed out this perl script:

        parse-df.pl

    This can be used as the basis for a model graph generator without the need of existing Cedar utilities.
    At a minimum this can work for the release CDROM (i.e. Cedar10.1). For older or newer versions,
    enhancements may be required. It's also intended as an easy to understand illustration for folks
    who have no prior experience with Cedar.

# find this project on github

    https://github.com/bjla93334/bjackson-cedarname.git
