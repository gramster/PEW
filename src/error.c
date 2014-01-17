/* error.c */

#include "misc.h"
#include "screen.h"
#include "gen.h"
#include "errors.h"
#include "help.h"
#include "keybd.h"

#include <stdarg.h>

static char ErrFilName[64], ErrHlpName[64];

#ifndef UNIX

void 
Error_Install(char *envvar, char *basename)
{
    char tmp[14];
    strcpy(tmp, basename);
    strcat(tmp, ".TBL");
    Gen_BuildName(envvar, tmp, ErrFilName);
    strcpy(tmp, basename);
    strcat(tmp, ".HLP");
    Gen_BuildName(envvar, tmp, ErrHlpName);
}

#endif				/* UNIX */

BOOLEAN 
    ErrorGetMessage(enum errornumber code, STRING buf, short buflen)
    {
	if (Gen_IGetFileEntry(ErrFilName, code, buf, buflen, FALSE) == NOFILE)
	    return FALSE;
	else
	    return TRUE;
    }

    void ErrorHelp(enum errornumber code)
    {
	char tmp[64];
	    strcpy(tmp, Hlp_FileName);
	    strcpy(Hlp_FileName, ErrHlpName);
	    Hlp_IHelp(code);
	    strcpy(Hlp_FileName, tmp);
    }

    void Error(BOOLEAN waitforkey, enum errornumber errno,...)
    {
	va_list args;
	char MsgBuf[80], ErrBuf[80];
	ushort rtn;
	    va_start(args, errno);
	if  (Gen_IGetFileEntry(ErrFilName, errno, MsgBuf, 80, FALSE) == NOFILE)
	        sprintf(ErrBuf, "Code: %d", errno);
	else
	        vsprintf(ErrBuf, MsgBuf, args);
	    va_end(args);
	    rtn = Win_Error(NULL, "", ErrBuf, FALSE, waitforkey);
	if  (rtn == KB_F1 || rtn == KB_ALT_H)
	        ErrorHelp(errno);
    }
