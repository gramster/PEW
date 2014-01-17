/* analyze.h */

#ifndef INCLUDED_ANALYZE
#define INCLUDED_ANALYZE

#ifndef INCLUDED_INTERP
#include "interp.h"
#endif

#define MAX_FIRE_ENTRIES	16

struct fire_entry		/* Every named transition gets one of these
				 * when its process is released */
{
    int trans_ident;		/* Transition name */
    int instance_count;		/* Number of such transitions */
    int fire_count;
};

typedef struct fire_entry fire_table[MAX_FIRE_ENTRIES];

#ifndef GLOBAL
#define GLOBAL extern
#endif

GLOBAL fire_table fire_entries;

GLOBAL BOOLEAN analyzing;

GLOBAL short Alz_Start, Alz_End, Alz_Step;
GLOBAL char Alz_Independent[32];
GLOBAL short next_free_fire_entry;

BOOLEAN DepEdit(void);
void analyze_init(void);
void gather_fire_entries(void);
void analyze_complete(void);
void analyze_update(transition_table tptr);

#endif				/* INCLUDED_ANALYZE */
