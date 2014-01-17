/********************************
*   gen.c - General Functions	*
********************************/

#include "misc.h"
#include "help.h"
/*#include "screen.h"*/
#include "keybd.h"
#include "errors.h"
#include "gen.h"
#include <errno.h>

#ifdef UNIX
int filelength(int fd);
char *strupr(char *);
#endif

/************************************************************************
* Gen_BuildName - build a name in 'dest' based on thebase name 'file'	*
*		   and the path in the environment variable 'env_var'.	*
************************************************************************/

void FAR 
Gen_BuildName(STRING env_var, STRING basename, STRING dest)
{
    int i;
    STRING tmp = getenv(env_var);
    if (tmp)
	strcpy(dest, tmp);
    else
	*dest = '\0';
    i = strlen(dest);
#ifdef UNIX
    if (i)
	if (dest[i - 1] != '/')
	    strcat(dest, "/");
#else
    if (i)
	if (dest[i - 1] != '\\')
	    strcat(dest, "\\");
#endif
    strcat(dest, basename);
}


/*****************************************
* The next function is a general purpose
* routine for reading messages from a
* number of different types of files, given
* an index. The name of the file, and location
* and size of the read buffer must also be
* specified.
*
* If the filename is specified without a
* suffix, the routine will attempt to open
* a file with the suffix ".idx". This is a
* direct index file, and should contain a
* number of longs, each specifying the file
* offset of the corresponding entry in a data
* file with suffix ".itx". This is the fastest
* method, but requires some kind of processing
* to set up these files correctly. Each entry
* in the ".itx" file should consist of a short
* length entry followed by the actual entry text.
*
* If there is no ".itx" file, or the filename
* included a suffix, an attempt to open the
* specified file is made. If successful, the
* first character is read. If this corresponds
* to INDEXCHAR, the file is assumed to be a
* sorted indexed file and a binary search is
* performed. If not, a straight linear search
* is done, and the nth line of the file read.
*
* In the former case, it is assumed the file
* has the layout:
*
*	@i1
*	<Text for index i1>
*	@i2
*	<text for index i2>
*	...
*	@in
*	<text for index in>
*
* where the indexes are arbitrary unique +ve integers
* or single alphabetic strings in ascending order,
* and the text may be spread over more than one line.
* In this case, the nl_to_spc flag will cause newlines
* to be converted to spaces.
*
* RETURN VALUES:
*	NOENTRY	- no entry found for index
*	NOFILE  - the data file could not be opened
*	BUFFOVRFLOW - the text exceeded the buffer size
*	BADXREF - a #see was found with no index
*	SUCCESS
*
***************************************************/

#define INDEXCHAR	'@'

static int 
_Gen_GetFileEntry(STRING filename, int Index, STRING Sndex,
		  STRING buffer, int bufsize, BOOLEAN nl_to_spc)
{
    FILE *fp, *ifp;
    int rtn;
    char tempname[80], *oldbuff;

    oldbuff = buffer;
SEARCH_AGAIN:
    buffer = oldbuff;
    fp = ifp = NULL;
    rtn = SUCCESS;
    if (Sndex == NULL && Index < 0)
    {
	rtn = NOENTRY;
	goto DONE;
    }
    strcpy(tempname, filename);
    if (strchr(tempname, '.') == NULL)
    {
	char *suffix = tempname + strlen(tempname);
	strcpy(suffix, ".idx");
	if ((ifp = fopen(tempname, "rb")) != NULL)
	{
	    strcpy(suffix, ".itx");
	    if ((fp = fopen(tempname, "rt")) != NULL)
	    {
		long pos;
		short len;
		fseek(ifp, sizeof(long) * Index, SEEK_SET);
		fread(&pos, sizeof(long), 1, ifp);
		fseek(fp, pos, SEEK_SET);
		fread(&len, sizeof(short), 1, fp);
		if (len >= bufsize)
		    rtn = BUFFOVRFLOW;
		else
		{
		    fread(buffer, sizeof(char), len, fp);
		    buffer += len;	/* skip to end */
		}
	    } else
		rtn = NOFILE;
	    goto DONE;
	} else
	    *suffix = '\0';
    }
    if ((fp = fopen(tempname, "rt")) == NULL)
    {
	rtn = NOFILE;
	goto DONE;
    }
    fgets(buffer, bufsize, fp);
    if (*buffer != INDEXCHAR)	/* straight sequential file */
    {
	while (--Index && !feof(fp))
	    fgets(buffer, bufsize, fp);
	rtn = (feof(fp)) ? NOENTRY : SUCCESS;
	buffer += strlen(buffer);	/* skip to end */
    } else
    {
	int top, bottom = 0, mid, tmp = 0, maxchklen = 0;
	top = filelength(fileno(fp));
	if (Sndex)
	{
	    Index = 0;
	    maxchklen = strlen(Sndex);
	    strupr(Sndex);
	}
	while (bottom <= top)
	{
	    mid = bottom + (top - bottom) / 2;
	    fseek(fp, mid, SEEK_SET);

	    do
	    {
		fgets(buffer, bufsize, fp);
	    } while (*buffer != INDEXCHAR && !feof(fp));

	    if (feof(fp))
		top = mid - 1;	/* reached EOF */
	    else
	    {
		if (Sndex)
		{
		    /* Do a case-insensitive compare */
		    int blen = 0;
		    while (blen < maxchklen)
		    {
			char c = buffer[blen + 1];
			tmp = toupper(c) - Sndex[blen];
			if (tmp)
			    break;
			blen++;
		    }
		    /* Even if we match, must still compare lengths... */
		    if (blen >= maxchklen && buffer[blen + 1] >= 32)
			tmp = 1;
		}
		/* numeric index - convert to integer */
		else
		    tmp = atoi(buffer + 1);
		if (tmp == Index)
		    break;
		else if (tmp > Index)
		    top = mid - 1;
		else
		    bottom = mid + 1;
	    }
	}
	if (tmp != Index)
	    rtn = NOENTRY;
	else
	    do
	    {			/* read the entry */
		int len;
		fgets(buffer, bufsize, fp);
		if (*buffer != INDEXCHAR)	/* not next entry */
		{
		    len = strlen(buffer);
		    if (buffer[len - 1] == '\n' && nl_to_spc)
			buffer[len - 1] = ' ';
		    buffer += len;
		    bufsize -= len;
		} else
		    break;
	    } while (!feof(fp));
    }
DONE:
    if (fp)
	fclose(fp);
    if (ifp)
	fclose(ifp);
    *buffer = 0;		/* terminate entry */
    if (strncmp(oldbuff, "#SEE ", 5) == 0)	/* got a see reference */
    {
	char *tmp = oldbuff + 5;
	while (*tmp && *tmp == ' ')
	    tmp++;
	if (*tmp)
	{
	    if (Sndex)
	    {
		char *tmp2 = tmp + strlen(tmp);
		while (*--tmp2 == ' ');	/* skip trailing space */
		*++tmp2 = 0;
		strcpy(Sndex, tmp);
	    } else
		Index = atoi(tmp);
	    goto SEARCH_AGAIN;
	} else
	    rtn = BADXREF;
    }
    return rtn;
}

short 
Gen_IGetFileEntry(STRING filename, short index, STRING buffer,
		  short bufsize, BOOLEAN nl_to_spc)
{
    return _Gen_GetFileEntry(filename, index, NULL, buffer, bufsize, nl_to_spc);
}

short 
Gen_SGetFileEntry(STRING filename, STRING index, STRING buffer,
		  short bufsize, BOOLEAN nl_to_spc)
{
    return _Gen_GetFileEntry(filename, 0, index, buffer, bufsize, nl_to_spc);
}
