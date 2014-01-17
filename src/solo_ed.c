/* Dummy routine for solo editor */

#include "misc.h"

short ec_errline;
short ec_errcol;

BOOLEAN 
run_compiler(char *name, int value)
{
    return TRUE;
}

/* This could be eliminated by using a pointer to a function in Ed-Compile
which is only called if non-NULL, is initialised to NULL, and gets set by
the PEW to point to the run_compiler routine in pew_ec. */

void 
execute_spec()
{
}
