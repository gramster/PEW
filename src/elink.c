/************************************************/
/*						*/
/*     link.c - ECode linker & optimiser	*/
/*						*/
/* written by Graham Wheeler, April 1989	*/
/*						*/
/************************************************/

#include "symbol.h"
#include "compiler.h"

short *arg;
short table[1000];		/* Allocate dynamically of correct size! */
static short sourceline;

static void NEAR 
next_instruction(void)
{
    arg = (short *)(emitbuff + in_address);
    op = (ecode_op) * arg;
    in_address += (int)Op_NArgs[(int)op] + 1;
}

static short NEAR 
displ(short ArgNum)
{
    if (arg[ArgNum] == -1)
	return -1;
    else if (table[arg[ArgNum]] == -1)
	return -1;
    else
	return table[arg[ArgNum]] - out_address;
}

static short NEAR 
value(short ArgNum)
{
    if (arg[ArgNum] == -1)
	return -1;
    else
	return table[arg[ArgNum]];
}

static void NEAR 
variable(short level, short disp)
{
    next_instruction();
    while (op == OP_FIELD)
    {
	disp += arg[1];
	next_instruction();
    }
    if (level == 0)
	if (op == OP_VALUE && arg[1] == 1)
	{
	    emit(OP_LOCALVALUE, disp);
	    next_instruction();
	} else
	    emit(OP_LOCALVAR, disp);
    else if (level == 1)
	if (op == OP_VALUE && arg[1] == 1)
	{
	    emit(OP_GLOBALVALUE, disp);
	    next_instruction();
	} else
	    emit(OP_GLOBALVAR, disp);
    else
	emit(OP_VARIABLE, level, disp);
}

static void NEAR 
process(short pass, short top)
{
    short start, size;
    emitcode = (pass == 2 ? TRUE : FALSE);
    out_address = in_address = sourceline = 0;
    next_instruction();

    while (in_address < top)
    {
	switch (op)
	{
	case OP_DEFADDR:
	    table[arg[1]] = out_address;
	    next_instruction();
	    break;
	case OP_DEFARG:
	    table[arg[1]] = arg[2];
	    next_instruction();
	    break;
	case OP_ASSIGN:
	    if (arg[1] == 1)
		emit(OP_SIMPLEASSIGN);
	    else
		emit(OP_ASSIGN, arg[1]);
	    next_instruction();
	    break;
	case OP_PROCEDURE:
	    emit(OP_PROCEDURE, value(1), value(2), displ(3));
	    next_instruction();
	    break;
	case OP_MODULE:
	    emit(OP_MODULE, arg[1], arg[2], arg[3], value(4), value(5), value(6), value(7),
		 value(8), arg[9], displ(10), displ(11), displ(12), displ(13), value(14));
	    next_instruction();
	    break;
	case OP_TRANS:
	    emit(OP_TRANS, value(1), value(2), value(3), value(4),
		 value(5), value(6), arg[7], arg[8], arg[9],
		 displ(10), displ(11), displ(12), displ(13),
		 arg[14], value(15), value(16), displ(17));
	    next_instruction();
	    break;
	case OP_EXISTMOD:
	    emit(op, arg[1], displ(2));
	    next_instruction();
	    break;
	case OP_FOR:
	case OP_NEXT:
	    emit(op, displ(1), arg[2]);
	    next_instruction();
	    break;
	case OP_ENTERBLOCK:
	    emit(op, value(1));
	    next_instruction();
	    break;
	case OP_GOTO:
	case OP_DO:
	    emit(op, displ(1));
	    next_instruction();
	    break;
	case OP_FIELD:
	    if (arg[1] != 0)
		emit(OP_FIELD, arg[1]);
	    next_instruction();
	    break;
	case OP_INDEXEDJUMP:
	    emit(OP_INDEXEDJUMP, displ(1));
	    next_instruction();
	    break;
	case OP_NEWLINE:
	    do
	    {
		sourceline = arg[1];
		next_instruction();
	    }
	    while (op == OP_NEWLINE);
	    emit(OP_NEWLINE, sourceline);
	    break;
	case OP_DEFIPS:
	    start = arg[1];
	    size = arg[2];
	    do
	    {
		next_instruction();
		if (op == OP_DEFIPS)
		{
		    if ((arg[1] + arg[2]) == start)
		    {
			start = arg[1];
			size += arg[2];
		    } else
			break;
		} else
		    break;
	    } while (TRUE);
	    emit(OP_DEFIPS, start, size);
	    break;
	case OP_PROCCALL:
	    if (arg[1] == 1)
		emit(OP_GLOBALCALL, displ(2), arg[3]);
	    else
		emit(OP_PROCCALL, arg[1], displ(2), arg[3]);
	    next_instruction();
	    break;
	case OP_STRINGCOMPARE:
	    if (arg[1] == 1)
		emit((ecode_op) arg[2]);
	    else
		switch (arg[2])
		{
		case OP_EQUAL:
		    emit(OP_STRINGEQUAL, arg[1]);
		    break;
		case OP_LESS:
		    emit(OP_STRINGLESS, arg[1]);
		    break;
		case OP_GREATER:
		    emit(OP_STRINGGREATER, arg[1]);
		    break;
		case OP_NOTEQUAL:
		    emit(OP_STRINGNOTEQUAL, arg[1]);
		    break;
		case OP_NOTLESS:
		    emit(OP_STRINGNOTLESS, arg[1]);
		    break;
		case OP_NOTGREATER:
		    emit(OP_STRINGNOTGREATER, arg[1]);
		    break;
		}
	    next_instruction();
	    break;
	case OP_VALUE:
	    if (arg[1] == 1)
		emit(OP_SIMPLEVALUE);
	    else
		emit(OP_VALUE, arg[1]);
	    next_instruction();
	    break;
	case OP_VARIABLE:
	    variable(arg[1], arg[2]);
	    break;
	case OP_INIT:
	    emit(OP_INIT, displ(1), arg[2], arg[3]);
	    next_instruction();
	    break;
	default:
	    emitarray((ushort *) arg);
	    next_instruction();
	    break;
	}
    }
}

#if !defined(INTEGRATE) || defined(WRITECODEFILE)
short 
Link(char *targetbasename, short top)
#else
short 
Link(short top)
#endif
{
#if defined(WRITECODEFILE) || !defined(INTEGRATE)

    FILE *targetfile;
    char targetname[16];
    extern void compiler_error(short, enum errornumber);
    strcpy(targetname, targetbasename);
    strcat(targetname, ".cod");
    if ((targetfile = fopen(targetname, "wb")) == NULL)
	compiler_error(0, ERR_NOTARGET);
#endif
    process(0, top);
    process(1, top);
    process(2, top);

#if defined(WRITECODEFILE) || !defined(INTEGRATE)

    fwrite(emitbuff, sizeof(short), (unsigned)out_address, targetfile);
    fclose(targetfile);
#endif
    return out_address;
}
