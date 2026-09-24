# VersionMap(s) - Cedar Release(s) [misnomer]

## Context

Warning : the term 'Kernel' is used here differently from Unix/Linux.

Cedar is used as a catch-all name for many different aspects of "the system" used for experimentation by Computer Science Laboratory members at Xerox Corporation's Palo Alto Research Center [i.e. Xerox PARC CSL].

## Experiment Programming Environment(s)

Let's start with the aspect that Cedar is an Environment for Experimental Software Development [@see EPE].  Creating software in CSL in the mid-80's was very different from working with other technologies from that era.  While Cedar evolved from concepts and techniques taken from the general community, and hammered out on the Alto, it took bold new steps in a new direction : a "strongly typed" and 'safe' runtime.

An overall goal was to make people highly productive.  In that vein,
"system crashes" were considered anathema  to everyone, and
the idea of being confronted with a "blue screen of death" was abhorred.
Instead, when a (checked) defect was encountered, users would be
provided with an expressive full debugger with full context, source code,
and access to all software structures of the "Abstract Machines" LoadState -
what is know these days as 'Reflection'.

To achieve this, the build system has to supply many features and operate
with many constraints, and provide a number of guarantees.

## Single Process (Task) World

As you may be aware, Cedar is a single address space (task) technology, with protection provided by the language's 'Kernel'.
The lowest level software for accessing hardware is hardened against failures and wrapped in a boundary dividing 'unsafe' operations from the majority of the code.
Everything runs together in a shared memory cooperative way and forms an integrated multi-threaded (Mesa process) 'World' of code.

The build system includes (in formal and ad-hoc) ways "System Modelling" features.
For a 'Release', the VersionMap is the core mechanism for capturing all files : sources, intermediates, and object-code that constitute that specific World.
File content is kept on a networked file server (@see Alto IFS), and necessary meta-data about the file set is compiled into a VersionMap so that access to files is efficient both for users and the machine (i.e. debugger).

Every file 'object' has a remote file server 'location' (in IFS syntax), and each file is uniquely identified by a 'version' and a "create date".
In addition, files have a 'seal' to guard against mistakes or tampering.

## Extensible World

Note : Worlds are also 'extensible'.
Software modules and applications be added, dynamically after the World's release.
Newly loaded code is checked for compatibility before it is added into the running environment.
Measures are taken to also allow for full debug-ability of layered software akin to the base release VersionMap.

## GDB

Contrast this with gdb and other language tools - which evolved much later - that use heuristics for locating object files, source files, and debugger symbols (a.out, COFF, ELF, etc).
With Cedar, everything comes for free, and you know what you've got explicitly.

## Remote Debugging

Oh, and toss in the additional ability for full remote debugging when the failure is bad that the World is unable to function well enough to provide the in-world debugging.

## eof
