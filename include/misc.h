/*********************************************************************
The GWLIBL library, its source code, and header files, are all
copyright 1987, 1988, 1989, 1990 by Graham Wheeler. All rights reserved.
**********************************************************************/

/**************************************/
/* misc.h - Miscellaneous Definitions */
/**************************************/

#ifndef INCLUDED_MISC
#define INCLUDED_MISC

#include <stdio.h>

#ifdef UNIX
#include <unistd.h>
#else
#include <alloc.h>
#include <dos.h>
#endif

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/**********************************/
/* Macro for declaration handling */
/**********************************/

#ifdef DECLARE
#define INIT(v,i)	v=i
#else
#define INIT(v,i)	extern v
#endif

/***************/
/* Basic Types */
/***************/

#ifdef UNIX
#define	near
#define far
#define huge
#undef TRUE
#undef FALSE
#else
#define HUGE		huge
#endif

#define NEAR		near
#define FAR		far

enum BooleanValues
{
    FALSE, TRUE
};

typedef char *STRING;
typedef char *STRINGARRAY[];

#if defined(UNIX) && !defined(SVR4)
typedef unsigned long ulong;
#endif

#ifndef UNIX
typedef unsigned long ulong;
typedef unsigned short ushort;
typedef unsigned int uint;
#endif
typedef unsigned char uchar;
typedef enum BooleanValues BOOLEAN;
typedef unsigned char byte;
typedef unsigned short word;

#define NUL	'\0'

/************************/
/* Far Pointer Handling */
/************************/

#define SEGMENT(p)	(ushort)(((ulong)((char far *)p)) >>16)
#define OFFSET(p)	(ushort)(((ulong)((char far *)p)) &0xFFFF)
#define FAR_PTR(s,o)	(char far *) (( (ulong)(s) << 16) + o)
#ifdef UNIX
#define PTR_TO_LONG(p)	((ulong)(p))
#else
#define PTR_TO_LONG(p)	((((ulong)(p)) >> 16l)*16l+ (ulong)((ushort)(p)))
#endif

/*********************/
/* Basic 'functions' */
/*********************/

#define DOS_C_SRV(n)       	((char)0xFF & (char)bdos(n,0,0))
#define getc_noecho()      	DOS_C_SRV(0x7)
#define check_keyboard()   	DOS_C_SRV(0xB)
#define ctrl(x)			(((char)x) & 037)
#define beep()			putchar(7)
#define get_tick()		(*((unsigned long far *)0x0000046CL))

/************************************************************/
/* Special memory handling routines with integrity checking */
/************************************************************/

#ifndef UNIX

void Mem_Init(void);
void Mem_Check(BOOLEAN free_table);
void Mem_Free(void *p);
void *Mem_Calloc(int nelts, int size, int who);
void *Mem_Malloc(int nbytes, int who);

#else

#define farcoreleft()		100000
#define max(a,b)		(((a)>(b))?(a):(b))
#define delay(d)
#define randomize()
#define getch()			getchar()
#define Mem_Init()
#define Mem_Check(f)
#define Mem_Free(p)		free(p)
#define Mem_Calloc(n,s,w)	calloc(n,s)
#define Mem_Malloc(n,w)		malloc(n)

extern char *itoa(), *ltoa();

#endif				/* UNIX */

#endif				/* INCLUDED_MISC */
