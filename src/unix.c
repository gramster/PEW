/************************************************************************/
/* unix.c - Replacements for Turbo C functions not supported under UNIX	*/
/************************************************************************/

#ifdef UNIX

#include <sys/stat.h>
#include "misc.h"
#include "screen.h"
WINPTR Win_Current;
STRING Hlp_FileName;

int 
filelength(int fd)
{
    int rtn = 0;
    struct stat sbuf;
    if (fstat(fd, &sbuf) == 0)
	rtn = sbuf.st_size;
    return rtn;
}

char *
strupr(char *str)
{
    char *rtn = str;
    while (*str)
    {
	if (*str >= 'a' && *str <= 'z')
	    *str += 'A' - 'a';
	str++;
    }
    return rtn;
}

/* Only base 10 and base 16 are supported */

char *
itoa(int val, char *str, int rad)
{
    if (rad == 10)
	sprintf(str, "%d", val);
    else if (rad == 16)
	sprintf(str, "%X", val);
    else
	strcpy(str, "ERR");
    return str;
}


char *
ltoa(long val, char *str, int rad)
{
    if (rad == 10)
	sprintf(str, "%ld", val);
    else if (rad == 16)
	sprintf(str, "%lX", val);
    else
	strcpy(str, "ERR");
    return str;
}

extern double drand48();

short 
myrandom(short range)
{
    return (short)(range * drand48());
}

#endif
