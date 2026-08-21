// quick and dirty :

enum flag_t { OPT_DEBUG, OPT_TRACE, OPT_DUMPENTRY, OPT_DUMPINDEX, OPT_DUMPSORTED, OPT_DUMPALL };

extern char *options[];
extern int o_flags;

#ifdef OPT_INIT
char *options[] = {
    "--debug",
    "--trace",
    "--dumpEntry",
    "--dumpIndex",
    "--dumpSorted",
    "--dumpAll",
    NULL
};

int o_flags = 0; // assumes only a few
#endif

__attribute__((always_inline))
inline bool opt_set(enum flag_t flavor) {
    return (o_flags & (1 << flavor));
}

extern void DumpIndex();
extern void DumpAll(int swap);
extern void DumpSorted(int swap);
