#include "misc.h"
#include "screen.h"
#include "errors.h"
#include "menu.h"
#include "ed.h"

void Ed_Main();
//char Ed_FileName[];
BOOLEAN demo_mode;

static void NEAR 
bad_usage(void)
{
    fputs("Usage: pew [-D <demoscript file name>]", stderr);
    fputs("   or: pew [<Estelle file name>]", stderr);
    exit(0);
}

void FAR 
Pew_EndSystem(short sig)
{
    Scr_EndSystem();
    Mem_Check(TRUE);
    exit(sig);
}

void 
main(argc, argv)
    int argc;
    char *argv[];
{
    Mem_Init();
#ifdef DEMO
    if (argc == 3)
    {
	if (strcmp(argv[1], "-D") == 0)
	{
	    extern demo_init(char *);
	    demo_mode = TRUE;
	    if (demo_init(argv[2]) == 0)
		bad_usage();
	} else
	    bad_usage();
    } else
#endif
    if (argc == 2)
	strcpy(Ed_FileName, argv[1]);
    else if (argc != 1)
	bad_usage();
    Scr_InitSystem();
    Error_Install("PEWPATH", "ERROR");
    Ed_Main();
    Pew_EndSystem(0);
}
