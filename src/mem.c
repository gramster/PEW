/*********************************************************************
The GWLIBL library, its source code, and header files, are all
copyright 1987, 1988, 1989, 1990 by Graham Wheeler. All rights reserved.
**********************************************************************/

#include <stdio.h>
#include <alloc.h>
#include "misc.h"
#include "errors.h"

/*
************************* MEMORY MANAGEMENT *****************************

 The memory management system uses two conditional compilation macros:

	USEOWNMEMBLOCK - tells the system to allocate a block of memory
			of size MEMSPACE bytes, and perform allocations
			from this block. Mem_Free then does nothing.
			This is useful if all allocated chunks can be freed
			in one go at the end, rather than individually.
	MEM_DEBUG - tells the system to include code to keep track of up
			to MEMTABLESIZE pointers, and check all attempts
			to free blocks for legality. Mem_Check can be
			called to report the location and size of all
			currently allocated memory chunks.

 The functions are:
	Mem_Init : Allocate and initialise the memory system. If neither
		of the conditional compiles are being used, this does
		nothing.
	Mem_Check : If MEM_DEBUG is defined, print details of all currently
		allocated chunks. If the Boolean parameter is true, the
		check table is freed up as well.
	Mem_Calloc : A `calloc' substitute. An extra parameter is used to
		identify the caller. This means that if MEM_DEBUG is set
		and an error occursin Mem_Free, the original allocation
		call can be identified.
	Mem_Malloc : As above, but a `malloc' workalike.
	Mem_Free: A `free' workalike, including error checking if MEM_DEBUG
		is defined.
*/

#ifdef USEOWNMEMBLOCK

#define MEMSPACE 	20000
static char *local_mem = NULL;
static int localmem_top;

#endif


#ifdef MEM_DEBUG

#define MEMTABLESIZE	2500

static struct MemTableEntry
{
    long p;
    int size, who;
}  *MemTable = NULL;

#endif


void 
Mem_Init(void)
{
#ifdef MEM_DEBUG
    int i;
    /* Allocate pointer tracking table and initialise it */
    if (MemTable == NULL)
	MemTable = (struct MemTableEntry *)calloc(MEMTABLESIZE, sizeof(struct MemTableEntry));
    /* else print warning */
    if (MemTable == NULL)
	Error(TRUE, ERR_MEMFAIL, MEMTABLESIZE, sizeof(struct MemTableEntry), 1000);
    else
	for (i = 0; i < MEMTABLESIZE; i++)
	{
	    MemTable[i].p = 0l;
	    MemTable[i].size = 0;
	}
#endif
#ifdef USEOWNMEMBLOCK
    /* Allocate block if necessary, and zero contents */
    if (local_mem != NULL)
	memset(local_mem, (char)0, MEMSPACE);
    else if ((local_mem = (char *)calloc(MEMSPACE, 1)) == NULL)
	Error(TRUE, ERR_MEMFAIL, MEMSPACE, 1, 1001);
    localmem_top = 0;
#endif
}

void 
Mem_Check(BOOLEAN free_table)
{
#ifdef MEM_DEBUG
    int i;
    if (MemTable)
    {
	for (i = 0; i < MEMTABLESIZE; i++)
	    if (MemTable[i].p != 0l)
		fprintf(stderr, "Memory block of %d bytes currently allocated at %08lX by %d\n",
			MemTable[i].size, MemTable[i].p, MemTable[i].who);
	if (free_table)
	    free(MemTable);
    }
#else
    (void)free_table;		/* Prevent a compiler warning */
#endif
}

void *
Mem_Calloc(int nelts, int size, int who)
{
    void *tmp;
    int rtn;
#ifdef USEOWNMEMBLOCK
    int nbytes = nelts * size, i;
    /*
     * Note that this is a very crude method, and does not support freeing of
     * blocks. It can be refined considerably.
     */
    if (nbytes >= (MEMSPACE - localmem_top))
	Error(TRUE, ERR_MEMFAIL, nelts, size, who);
    else if (nbytes == 0)
	return NULL;
    else
	rtn = localmem_top;
    localmem_top += nbytes;
    tmp = (void *)&(local_mem[rtn]);
#else
    tmp = calloc(nelts, size);
    if (tmp == NULL)
	Error(TRUE, ERR_MEMFAIL, nelts, size, who);
#endif
#ifdef MEM_DEBUG
    if (MemTable)
    {
	for (i = 0; i < MEMTABLESIZE; i++)
	{
	    if (MemTable[i].p == 0l)
	    {
		MemTable[i].p = PTR_TO_LONG(tmp);
		MemTable[i].size = nbytes;
		MemTable[i].who = who;
		break;
	    }
	}
	if (i >= MEMTABLESIZE)
	    Error(TRUE, ERR_MEMTABLEFULL, nbytes, PTR_TO_LONG(tmp));
    }
#endif
    return tmp;
}

void *
Mem_Malloc(int nbytes, int who)
{
    return Mem_Calloc(nbytes, 1, who);
}

void 
Mem_Free(void *p)
{
#ifdef MEM_DEBUG
    int i;
    long pv;
    if (p && MemTable)
    {
	pv = PTR_TO_LONG(p);
	for (i = 0; i < MEMTABLESIZE; i++)
	{
	    if (MemTable[i].p == pv)
	    {
		MemTable[i].p = 0l;
		MemTable[i].size = 0;
		break;
	    }
	}
	if (i >= MEMTABLESIZE)
	    Error(TRUE, ERR_BADFREE, pv);
    }
#endif

#ifdef USEOWNMEMBLOCK
    /* Do nothing */
#else
    if (p)
	free(p);
#endif
}
