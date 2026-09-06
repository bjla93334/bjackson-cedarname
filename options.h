// quick and dirty :

// order is import here
enum flag_t { OPT_DEBUG, OPT_TRACE,
    OPT_DUMP_ALL,
    OPT_DUMP_ENTRY,
    OPT_DUMP_INDEX,
    OPT_DUMP_PREFIXMAP,
    OPT_DUMP_SORTED,
    OPT_DUMP_STAB,
    OPT_MAP_FILE
};

extern char *options[];
extern int o_flags;

#ifdef OPT_INIT
char *options[] = {
    "--debug",
    "--trace",
    "--dumpAll",
    "--dumpEntry",
    "--dumpIndex",
    "--dumpPrefixMap",
    "--dumpSorted",
    "--dumpStab",
    "--mapFile",
    NULL
};
int o_flags = 0; // assumes only a few
#endif

__attribute__((always_inline))
inline bool opt_set(enum flag_t flavor) {
    return (o_flags & (1 << flavor));
}

extern void setMapName(char *localFSName);

extern void DumpAll();
extern void DumpIndex();
extern void DumpPrefixMap();
extern void DumpSorted();
extern void DumpStab();
