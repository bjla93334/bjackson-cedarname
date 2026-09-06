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

A location (aka pathname) is a bit complicated, informally think of it as being an IFS name of the form :
    [server]<directory>subdirs>shortname!version

The shortname is further structured as a (base, ext) pair separted by a dot (.) which base doesn't contain any dots, but ext may (ugh/yuck).  For the purposes of 'cedarname' we only have to worry about the shortname.

As you might expect - everyone, even Cedar, has their own notion of time.  Thankfully, Cedar is similar to unix and instances of time have integer values, representing a (unique) distance / interval from a fixed 'epoch' value (i.e. 0).  Sadly, Cedar's BasicTime epoch differs from the de-facto unix value, so an adjustment value is added to compensate so that we can use unix/posix utilities to express the numeric value as a "calendar string" which users prefer (!!) when trying to understand when instances of time happened.

Version Stamps are GUID's, which are constructed (pseudo) 'randomly'.
There seems to be some fuzzyness around how various Cedar releases implemented GUIDs,
for the present release (Cedar 10.1), 64 bits are available in the data, but only 32 bits (num and hi) are non-zero.

# VersionMap data

Ignoring for the moment byte-order issues (network vs. host, big vs little),
The VersionMap file (data) has 3 sections : the shortname 'index', the file entry list, and a compact character table (stab).  Oh, and the first "line" which indicates how many entries there are.

The file entry list contains the tuples, with the location string represented as a numeric reference into the stab so that the records are fixed length.  There's a trick being done with the character table where the variable llength strings are a sequential log, and there's an extra (null) file entry that supplies the "following index" for the last location string.  So, location string length is computed by using the 'start' from the file entry and the "just after" from the successor file entry.  This means you don't have to compute how long the character table is - that's done once when the file is read into memory.

The file entry list is kept in 'stamp' order, and the shortname list is kept in alphabetical order.
The 2 maps can therefore be quickly accessed using a binary search algorithm.

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

# Quick test

There's two very obvious things folks will be interested in,
so let's use them for testing :

    ./cedarname /r/PFS.mesa
    ./cedarname /r/Rope.mesa

    ./cedarname --dumpEntry /r/Tioga.tip
    ./cedarname --dumpAll
    ./cedarname --dumpIndex
    ./cedarname --dumpSorted
    ./cedarname --dumpPrefixMap
    ./cedarname --dumpStab

# ugh, other map file format(s)

    ./cedarname --debug --mapFile data/CedarSource.VersionMap\!34  /r/Rope.mesa

# Bah!

/cyan/cedar6.1/versionmap/VersionMapImpl.mesa!1

od -t x1 data/CedarSource.VersionMap\!34 | head
0000000 e3 de 01 2e 05 dc 00 00 00 22 9e 44 6d a2 20 3e

version: [dee3 2e01],
nEntries: [dc05 0000],
map.entries,
map.shortNameSeq,
namesChars

MyVersion: INT = 19850206; -- 012E.E3DE
    Every saved version stamp file needs this number at its start.
    We got this number from the date February 6, 1985, and
    we suggest that future versions also use this convention for generating this number.

SaveMapToFile: PUBLIC PROC [map: Map, name: ROPE] = TRUSTED {
    st: IO.STREAM ← FS.StreamOpen[name, $create];
    names: ROPE ← map.names;
    namesChars: INT ← names.Size[];
    len: NAT ← map.len;

    myVersion: INT ← MyVersion;
    IO.UnsafePutBlock[st, [LOOPHOLE[LONG[@myVersion]], 0, SIZE[INT]*bytesPerWord]];
        -- first, output the internal version number
    IO.UnsafePutBlock[st, [LOOPHOLE[LONG[@len]], 0, SIZE[NAT]*bytesPerWord]];
        -- first, output the # of entries
    IO.UnsafePutBlock[st, [LOOPHOLE[@map.entries[0]], 0, len*(SIZE[MapEntry]*bytesPerWord)]];
        -- next, the entries themselves
    IO.UnsafePutBlock[st, [LOOPHOLE[@map.shortNameSeq[0]], 0, len*(SIZE[CARDINAL]*bytesPerWord)]];
        -- next, the shortNameSeq
    IO.UnsafePutBlock[st, [LOOPHOLE[LONG[@namesChars]], 0, (SIZE[INT]*bytesPerWord)]];
        -- next, the number of characters we are about to write (not counting the end marker)
    IO.PutRope[st, names];
        -- then the names rope
    IO.PutRope[st, "\000\000\000"];
        -- marker at end to make Tioga happy
    IO.SetLength[st, IO.GetIndex[st]]; -- force goddamm truncation already!!!
    IO.Close[st];
};

# Implementor Notes - Archaeology or Forensics ??

    The 'stab' (Index) is NOT paired with the Entry in dumpAll.
    Don't think too hard about it - just think slice 'i' of independent tables.

    ./cedarname --dumpSorted | egrep 'canon|utc' | less
    ./cedarname --dumpAll | egrep 'canon|stamp' | sed -e 's| 0000 lo:.*||' | less
    ./cedarname --dumpAll | egrep ' i:|stab:' > /tmp/stab-order
    ./cedarname --dumpStab | sort -n -k2 | less

    Hmmm, stab ordering isn't what I expected ??
    Symbol Table starts with : "[Cedar10.1]<Top>", and does end at MMMKeyboard.mesa!2
    there's CR's between values

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
