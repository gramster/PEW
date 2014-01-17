/*****************************/
/* gen.h - General Functions */
/*****************************/

#ifndef INCLUDED_GEN

#include "screen.h"
#include "help.h"

#define INCLUDED_GEN

/*************************************/
/* Return values of Gen_GetFileEntry */
/*************************************/

#define SUCCESS		0
#define NOFILE		-1
#define NOENTRY		-2
#define BUFFOVRFLOW	-3
#define BADXREF		-4

/********************/
/* Global variables */
/********************/

#ifndef GLOBAL
#define GLOBAL extern
#endif

GLOBAL short Debugging, Debug2File;

#ifndef UNIX
#include "screen.h"
GLOBAL WINPTR Gen_MessageWindow;
#endif

/**************/
/* Prototypes */
/**************/

short Gen_HasDiskChanged(short disk);
void Gen_ShowMessage(STRING message);
void Gen_ClearMessage(void);
void Gen_BuildName(STRING env_var, STRING basename, STRING dest);

void 
Gen_EditString(ushort c, STRING start, STRING * end, STRING * now,
	       BOOLEAN * mode, short maxlen);

short 
Gen_IGetFileEntry(STRING filename, short index, STRING buffer,
		  short bufsize, BOOLEAN nl_to_spc);
short 
Gen_SGetFileEntry(STRING filename, STRING index, STRING buffer,
		  short bufsize, BOOLEAN nl_to_spc);

short 
Gen_IsMatch(STRING txt, STRING pat, short end, BOOLEAN CaseSensitive,
	    BOOLEAN IdentsOnly);

STRING Gen_ExpandTabs(STRING txt, STRING dest, short tabsize);
STRING Gen_CompressSpaces(STRING txt, STRING dest, short tabsize);

void Gen_Suspend(STRING progname);
ushort 
Gen_GetResponse(WINPTR w, STRING prompt, STRING response,
		BOOLEAN showresponse, short resplen, enum helpindex help);
void Gen_Debug(STRING fmt,...);
void Gen_Format360(short drive);
void Gen_WriteSector(short drive, short head, short track, short sector, uchar * buf);
void Gen_Encrypt(STRING in, STRING out);

#endif				/* INCLUDED_GEN */
