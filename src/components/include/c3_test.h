#define NO_EXAMINE
//#define EXAMINE_SCHED
//#define EXAMINE_MM
//#define EXAMINE_RAMFS
//#define EXAMINE_TE
//#define EXAMINE_LOCK
//#define EXAMINE_EVT

#ifdef EXAMINE_RAMFS
#define TEST_RAMFS_C3    // using cbufp version of treadp and twritep
#endif

#ifdef EXAMINE_MM
#define MM_C3            // enable upcall to each client to recover the pages
#endif
