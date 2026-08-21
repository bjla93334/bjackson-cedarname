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
    ./cedarname --dumpIndex
    ./cedarname --dumpAll
    ./cedarname --dumpSorted

# Implementor Notes

    The 'stab' (Index) is NOT paired with the Entry in dumpAll.
    Don't think too hard about it - just think slice 'i' of independent tables.

    ./cedarname --dumpSorted | egrep 'canon|utc' | less

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
