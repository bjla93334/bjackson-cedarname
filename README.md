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

# Original Definitive Reference for DF's - CSL-82-7, pg 85, Fig 4.3

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

# find this project on github

    https://github.com/bjla93334/bjackson-cedarname.git
