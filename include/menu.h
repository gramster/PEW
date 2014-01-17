/*********************************************************************
The GWLIBL library, its source code, and header files, are all
copyright 1987, 1988, 1989, 1990 by Graham Wheeler. All rights reserved.
**********************************************************************/

#ifndef INCLUDED_MENU
#define INCLUDED_MENU

/***********************************************/
/* Menu structures and function template	*/
/***********************************************/

/* Basic Types */

#define MN_TOGGLE	1
#define MN_VALUE	2
#define MN_COMMAND	3
#define MN_SUBMENU	4

/* Modifiers */

#define MN_STRING	8	/* return value is type string (else int) */
#define MN_EXIT		16	/* Exit menu after selection? */
#define MN_FIXED	32	/* Non-modifiable item; selection is same as
				 * pressing F1 */
#define MN_SPECIAL	64	/* Requires exceptional processing; selection
				 * results in auxiliary function being called
				 * (like MN_COMMAND, but not tied to
				 * MN_COMMAND restrictions). */
#define MN_SKIP		128	/* A bit of a misnomer; these options are for
				 * separators and information and cannot be
				 * highlighted */
#define MN_HEX		256	/* Numeric value option is in hexadecimal */


/* A Mnu_Opt corresponds to a single menu parameter */

typedef struct Mnu_Opt
{
    char *Item;			/* The parameter name (eg "Baud Rate")	 */
    ushort type;		/* The option type			 */
    enum helpindex help;	/* Index of help string			 */
    struct Menu *submenu;	/* Pointer to submenu			 */
        BOOLEAN(*aux) (void *);	/* Pointer to auxiliary function	 */
    short maxchoices;		/* The number of possible legal values	 */
    short choice;		/* The array index of the current value	 */
    void *value;		/* The pointer to the current value	 */
    STRINGARRAY *p_vals;	/* The (printable) set of values	 */
}   MENUOPT;

typedef MENUOPT MENUOPTARRAY[];

/* nopts should be calculated using sizeof, to eliminate the need to
change menu struct definitions when adding/removing options */

typedef struct Menu
{
    short y,			/* Top row of menu window		 */
        x,			/* Left column of menu window		 */
        h,			/* Height of menu window		 */
        w,			/* Width column of menu window		 */
        border,			/* Number of border lines - 0, 1 or 2	 */
        titlealign,		/* Alignment of menu title		 */
        textalign;		/* Alignment of option names		 */
    uchar ia,			/* Inside attribute			 */
        ha,			/* Highlight attribute			 */
        ba;			/* Border attribute			 */
    short nopts,		/* Number of options in array		 */
        optnow,			/* Current option			 */
        optx;			/* Column at which to print value field	 */
    char *title;		/* Menu title				 */
    MENUOPTARRAY *Opts;		/* Pointer to the array of options	 */
}   MENU;

#define SUBMENU		MENU
#define SUBMENUOPT	MENUOPT

typedef MENU *MENUPTR;

#ifdef UNIX
#define Mnu_Process(m,r,t)	0
#else

ushort Mnu_Process(MENUPTR menu, WINPTR responseWin, int timeout);

#endif				/* UNIX */

#endif				/* INCLUDED_MENU */
