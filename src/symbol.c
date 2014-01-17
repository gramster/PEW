#include "misc.h"
#include "screen.h"

#undef GLOBAL
#define GLOBAL
#include "symbol.h"
#undef GLOBAL

#include "compiler.h"
#include "interp.h"

#define PERC(x,t)       (short)((100l*(long)x) / (long)t)	/* Percentage */

#define MAXKEY		631	/* Number of hash table entries		 */

/********************/
/* Type definitions */
/********************/

typedef struct wordrecord *wordpointer;

struct wordrecord
{
    wordpointer nextword;
    BOOLEAN is_ident;
    short IDindex;		/* Identifier index */
};

static short characters = 0;

static wordpointer hash[MAXKEY];

/* ID_MapTable maps Id's into ID_NameStore for symbolic support	*/

short ID_MapTable[MAXIDENTIFIERS];
char ID_NameStore[MAXCHAR];

#if !defined(UNIX) || defined(EC_EI)
static char *_ClassNames[] =
{
    "Systemprocess", "SP",
    "Systemactivity", "SA",
    "Process", "Pr",
    "Activity", "Ar"
};
#endif

char *locspace, *locspacenow;
struct blockrecord block[MAXLEVEL + 1];
int blocklevel, out_address, in_address, variable_scope_base;
short idents;
short ID_MapTable[MAXIDENTIFIERS];
char ID_NameStore[MAXCHAR];

char *
ClassName(processclass c, BOOLEAN shortform)
{
    char *rtn = "";
#if !defined(UNIX) || defined(EC_EI)
    if (c == CLASS_UNATTRIB)
	rtn = shortform ? "Un" : "Unattributed";
    else
	rtn = _ClassNames[(int)c * 2 + (shortform ? 1 : 0)];
#endif
    return rtn;
}


/**********************************************/
/* Compiler Memory Allocation from local heap */
/**********************************************/

#if defined(MAKING_EC) || defined(INTEGRATE) || defined(EC_EI)

char *
EC_malloc(short nbytes)
{
    char *rtn = locspacenow;
    locspacenow += nbytes;
    if (((ulong) locspacenow - (ulong) locspace) > (ulong) LOC_SPACE_SIZE)
	compiler_error(0, ERR_SYMBOLSPC);
#ifdef UNIX
    return rtn;
#else
    return (char FAR *)((char HUGE *)rtn);
#endif
}

void 
Sym_Initialise()
{
    short keyno, i;
    unsigned long far *p = (ulong far *) locspace;
    i = (32768 - 4096) / sizeof(long);
    while (i--)
	*p++ = 0l;		/* Clear vid mem */
    for (keyno = 0; keyno < MAXKEY; keyno++)
	hash[keyno] = NULL;
    for (i = 1; i < MAXIDENTIFIERS; i++)
	ID_MapTable[i] = -1;
    variable_scope_base = characters = 0;
}

#endif

/************************************/
/* Internal Symbol Table Management */
/************************************/

#if defined(MAKING_EC) || defined(INTEGRATE) || defined(EC_EI)

static short NEAR 
key(STRING keytext)
{
    short sum = 0;
    while (*keytext)
    {
	sum += *keytext++;
	sum %= 32641 /* 32768-127 */ ;
    }
    return sum % MAXKEY;
}


void 
insert(BOOLEAN is_ident, STRING inserttext, short IDindex)
{
    wordpointer Pointer;
    char *where = ID_NameStore + characters;
    short start = characters, keyno;
    keyno = key(inserttext);
    while (*inserttext)
    {
	*where++ = *inserttext++;
	characters++;
    }
    *where = 0;
    characters++;
    if (characters >= MAXCHAR)
	compiler_error(0, ERR_SPECTOOBIG);
    Pointer = (wordpointer) EC_malloc(sizeof(struct wordrecord));
    Pointer->nextword = hash[keyno];
    Pointer->is_ident = is_ident;
    Pointer->IDindex = IDindex;
    ID_MapTable[IDindex] = start;
    hash[keyno] = Pointer;
}



short 
findID(stringtype searchtext)	/* Get index given name */
{
    wordpointer Pointer;
    short keyno = key(searchtext);
    Pointer = hash[keyno];
    while (Pointer)
    {
	if (strcmp(searchtext, ID_NameStore + ID_MapTable[Pointer->IDindex]) == 0)
	    break;
	else
	    Pointer = Pointer->nextword;
    }
    if (Pointer)
	return Pointer->IDindex;
    else
	return 0;
}

short 
search(stringtype searchtext, BOOLEAN * is_ident)
{
    wordpointer Pointer;
    BOOLEAN done = FALSE;
    short IDindex = 0, keyno = key(searchtext);
    Pointer = hash[keyno];
    while (!done)
    {
	if (Pointer == NULL)
	{
	    *is_ident = TRUE;
	    insert(TRUE, searchtext, IDindex = ++idents);
	    done = TRUE;
	} else if (strcmp(searchtext, ID_NameStore + ID_MapTable[Pointer->IDindex]) == 0)
	{
	    *is_ident = Pointer->is_ident;
	    IDindex = Pointer->IDindex;
	    done = TRUE;
	} else
	    Pointer = Pointer->nextword;
    }
    return IDindex;
}

#endif				/* INTEGRATE || MAKING_EC */

static pointer NEAR 
stabsearch(short ident, short levelno)
{
    pointer object = block[levelno].lastobject;
    while (object)
    {
	if (object->ident == ident)
	    break;
	object = object->previous;
    }
    return object;
}

pointer 
find_definition(short ident)
{
    pointer object = NULL;
    short levelno = blocklevel + 1;
    while (--levelno >= 0)
    {
	if ((object = stabsearch(ident, levelno)) != NULL)
	{
	    if (object->kind == VARIABLE)
		if (levelno < variable_scope_base)
		    object = NULL;
	    break;
	}
    }
    return object;
}

#if defined(INTEGRATE) || defined(MAKING_EC) || defined(EC_EI)

pointer 
    define(short ident, enum class kind)
    {
	pointer object;
	if  (ident != NOIDENTIFIER)
	    if  (stabsearch(ident, blocklevel))
		    compiler_error(6, ERR_AMBIGUOUS);
	    object = (pointer) EC_malloc(sizeof(struct objectrecord));
	    object->ident = ident;
	    object->previous = block[blocklevel].lastobject;
	    object->kind = kind;
	    object->type = NULL;
	    block[blocklevel].lastobject = object;
	    return object;
    }

#endif

#ifndef MAKING_EC

/* get_symbolic_name sometimes fails to find the symbol table entry,
	due, I think, to the scope activation not being set up properly
	for the identifier before searching for it. This doesn't prevent
	it from finding the name, only the array subscript(s).
*/

    void get_symbolic_name(short ident, ushort index, char *name, char *zero_val, short bufsize)
{
#if defined(UNIX) && !defined(EC_EI)
    return;
#endif
    if (buildBG)
	return;
    if (ident == 0)
	if (zero_val)
	{
	    strncpy(name, zero_val, (unsigned)bufsize);	/* several */
	    name[bufsize - 1] = 0;
	} else
	    *name = 0;
    else if (ident == -1)
	strcpy(name, "-");
    else
    {
	pointer obj;
	short offset;
	strncpy(name, ID_NameStore + ID_MapTable[ident], (unsigned)bufsize);
	name[bufsize - 1] = 0;
	bufsize -= 1 + (int)strlen(name);
	obj = find_definition(ident);
	if (obj && obj->type != NULL && obj->type->kind == ARRAYTYPE)
	{
	    /* Reverse calculate array indices */
	    short i = 0, size = 1, antisize = 1, top[6], bot[6], dim[6],
	        val[6], dims = 0;
	    offset = index - obj->place;
	    obj = obj->type;
	    while (obj->kind == ARRAYTYPE)
	    {
		top[i] = obj->_Aray_UpperVal;
		bot[i] = obj->_Aray_LowerVal;
		dim[i] = top[i] - bot[i] + 1;
		size *= dim[i];
		i++;
		obj = obj->type;
	    }
	    dims = i;
	    while (--i >= 0)
	    {
		short tmp;
		size /= dim[i];
		tmp = offset / size;
		val[i] = bot[i] + tmp;
		offset -= tmp * antisize;
		antisize *= dim[i];
	    }
	    if (bufsize-- > 0)
		strcat(name, "[");
	    for (i = 0; i < dims; i++)
	    {
		char tmp[8];
		short tmplen;
		(void)itoa(val[i], tmp, 10);
		tmplen = (int)strlen(tmp);
		if (bufsize >= tmplen)
		{
		    strcat(name, tmp);
		    bufsize -= tmplen;
		}
		if (i < (dims - 1))
		    if (bufsize-- > 0)
			strcat(name, ",");
	    }
	    if (bufsize-- > 0)
		strcat(name, "]");
	}
    }
}

/* buff must be 23 bytes at least! Also assumes transition's parent
	is in modnow. */

void 
Get_TransName(TRANS * tptr, char *buff, BOOLEAN fordisplay)
{
    char fromstr[10], tostr[10];

    if (tptr->fromident == -1)
	strcpy(fromstr, "ANY");
    else
	get_symbolic_name(tptr->fromident, 0, fromstr, "...", 10);

    if (tptr->toident == -1)
	strcpy(tostr, "SAME");
    else
	get_symbolic_name(tptr->toident, 0, tostr, "SAME", 10);

    if (fordisplay)
	sprintf(buff, "%9s %c %-9s", fromstr, 26, tostr);
    else
	sprintf(buff, "%9s -> %-9s", fromstr, tostr);
}

/* Must be 22 bytes! */

void 
Get_ModuleName(MODULE * mod, short index, char *buff)
{
    MODULE *oldmod = modnow;
    if (mod == root)
	strcpy(buff, "Specification");
    else
    {
	short v;
	activate_module(mod->parent);
	v = modnow->modvar_tbl[index].varident;
	/* activate_module(mod); */
	get_symbolic_name(v, index, buff, NULL, 22);
	activate_module(oldmod);
    }
}


void 
Get_ModvarName(MODULE * mod, char *buff)
{
    Get_ModuleName(mod, mod->index, buff);
}


static char NEAR 
tr_char(TRANS * tptr, ushort hasflg, ushort okflg)
{
    char rtn;
    if (Bit_Set(tptr->flags, hasflg))
	if (Bit_Set(tptr->flags, okflg))
	    rtn = '1';
	else
	    rtn = '0';
    else
	rtn = '-';
    return rtn;
}

/* buff must be >= 49 bytes */
/* The ForDisplay business is beacuse the arrow character is CTRL-Z
	which TurboC stupidly uses as the End-of-File indicator; thus
	if I use in the log file it I cannot load the log into the editor */

void 
Get_TransStatusLine(ushort tr_num, char *buff, BOOLEAN ForDisplay)
{
    char tmpbuff[24];
    TRANS *tptr;
    tptr = modnow->trans_tbl + tr_num;
    Get_TransName(tptr, tmpbuff, ForDisplay);
    if (tptr->priority == -1)
	strcpy(buff, "     ");
    else
	sprintf(buff, "%5d", tptr->priority);
    sprintf(buff + 5, "%c %20s W:%c P:%c D:%c E:%c S:%c\n", Bit_Set(tptr->flags, EXECUTING) ? '*' : ' ',
	    tmpbuff, tr_char(tptr, HAS_WHEN, WHEN_OK),
       tr_char(tptr, HAS_PROV, PROV_OK), tr_char(tptr, HAS_DELAY, DELAY_OK),
	    Bit_Set(tptr->flags, FIREABLE) ? '1' : '0', Bit_Set(tptr->flags, SELECTED) ? '1' : '0');
}

/* buff must be larger than 68 bytes */

void 
Get_TransStatLine(char *buff, ushort tr_num, BOOLEAN ForDisplay)
{
    float back_delay, self_delay;
    char transstr[24];
    TRANS *tptr = modnow->trans_tbl + tr_num;
    Get_TransName(tptr, transstr, ForDisplay);
    if (tptr->fire_count > 0)
    {
	back_delay = ((float)tptr->sumdelay) / ((float)tptr->fire_count);
	self_delay = ((float)tptr->selfdelay) / ((float)tptr->fire_count);
	tptr->forward_delay = ((float)tptr->nextdelay) / ((float)tptr->fire_count);
    } else
	self_delay = back_delay = tptr->forward_delay = 0.;
    sprintf(buff, "%20s%5d  %5d %5ld %5ld %7.3f %7.3f %7.3f\n", transstr,
	    (int)tptr->enbl_count, (int)tptr->fire_count,
	    (long)tptr->first_tm, (long)tptr->last_tm, back_delay,
	    self_delay, tptr->forward_delay);
}

/* Buff must be >= 78 bytes */

void 
Get_IPStatLine(char *buff, ushort ip_num, BOOLEAN dummy)
{
    IP *ip = Ip(modnow, ip_num);
    ulong mean_len = 0, mean_time = 0, time = globaltime - modnow->init_time;
    if (time != 0l)
	mean_len = (ip->total_length / time);
    if (ip->total_ints != 0)
	mean_time = (ip->total_time / (long)ip->total_ints);
    sprintf(buff, "%-15s %5d            %5ld       %5d      %5ld      %5d\n",
	    ip->name, (int)ip->total_ints, mean_len, (int)ip->max_length, mean_time, (int)ip->max_time);
    (void)dummy;		/* prevent compiler warning */
}

#endif				/* MAKING_EC */

/********************************/
/* Final compilation statistics */
/********************************/

#if defined(MAKING_EC) || defined(EC_EI)

void 
show_stats(time_t start_time, short codesize, BOOLEAN has_error)
{
    short stabsize = (unsigned)locspacenow - (unsigned)locspace;
    time_t end_time;
    time(&end_time);
    if (has_error)
	puts("Compilation aborted due to error");
    else
	puts("Compilation complete - no errors found");
    printf("Code        : %d bytes\n", (int)(codesize << 1));
    printf("Symbol Table: %d bytes (%d%%)\n", (int)stabsize, PERC(stabsize, LOC_SPACE_SIZE));
    printf("String Store: %d bytes (%d%%)\n", (int)characters, PERC(characters, MAXCHAR));
    printf("Identifiers : %d\n", (int)idents);
    printf("Time        : %d secs\n", (int)(end_time - start_time + 1l));
}

void 
dump_idents(void)
{
    short i;
    for (i = 1; i <= idents; i++)
	if (ID_MapTable[i] != -1)
	    printf("%3d -> %s\n", i, ID_NameStore + ID_MapTable[i]);
}

void 
print_stab_level(short level)
{
    pointer obj = block[level].lastobject;
    printf("Symbol Table (level %d at %ld)\n\n", (int)level,
	   (PTR_TO_LONG(obj) - PTR_TO_LONG(locspace)));
    while (obj)
    {
	if (obj->ident)
	{
	    char *n = "";
	    printf("Ident: %3d  Location: %5ld  Place: %3d  Kind:",
		   obj->ident, (PTR_TO_LONG(obj) - PTR_TO_LONG(locspace)), obj->place);
	    switch (obj->kind)
	    {
	    case ARRAYTYPE:
		n = "Arraytype";
		break;
	    case BODY:
		n = "Body";
		break;
	    case CONSTANTX:
		n = "Constantx";
		break;
	    case CHANNELTYPE:
		n = "Channeltype";
		break;
	    case ENUMERATEDTYPE:
		n = "Enumeratedtype";
		break;
	    case FIELD:
		n = "Field";
		break;
	    case IPTYPE:
		n = "Iptype";
		break;
	    case IP_:
		n = "Ip";
		break;
	    case INTERACTION_:
		n = "Interaction";
		break;
	    case INTERACTIONARG:
		n = "InteractionArg";
		break;
	    case MODULETYPE:
		n = "Moduletype";
		break;
	    case MODVAR:
		n = "Modvar";
		break;
	    case POINTERTYPE:
		n = "Pointertype";
		break;
	    case PROCEDUR:
		n = "Procedur";
		break;
	    case RECORDTYPE:
		n = "Recordtype";
		break;
	    case ROLETYPE:
		n = "Roletype";
		break;
	    case SETTYPE:
		n = "Settype";
		break;
	    case STANDARDFUNC:
		n = "Standardfunc";
		break;
	    case STANDARDPROC:
		n = "Standardproc";
		break;
	    case STANDARDTYPE:
		n = "Standardtype";
		break;
	    case STATEX:
		n = "State";
		break;
	    case STATESETX:
		n = "Stateset";
		break;
	    case SUBRANGETYPE:
		n = "Subrangetype";
		break;
	    case DERIVEDTYPE:
		n = "Derivedtype";
		break;
	    case UNDEFINED:
		n = "Undefined";
		break;
	    case VARIANTTYPE:
		n = "Varianttype";
		break;
	    case VARIABLE:
		n = "Variable";
		break;
	    case VALUEPARAMETER:
		n = "Valueparameter";
		break;
	    case VARPARAMETER:
		n = "Varparameter";
		break;
	    }
	    printf("%-16s   Name: %s\n", n, ID_NameStore + ID_MapTable[obj->ident]);
	}
	obj = obj->previous;
    }
}

void 
Sym_Save(char *basename)
{
    char fullname[80];
    FILE *sym_fp;
    strcpy(fullname, basename);
    strcat(fullname, ".sym");
    sym_fp = fopen(fullname, "wb");
    fwrite(hash, sizeof(wordpointer), MAXKEY, sym_fp);
    fwrite(ID_MapTable, sizeof(short), MAXIDENTIFIERS, sym_fp);
    fwrite(ID_NameStore, sizeof(char), MAXCHAR, sym_fp);
    fwrite(locspace, sizeof(char), LOC_SPACE_SIZE, sym_fp);
    fclose(sym_fp);
}

#endif				/* MAKING_EC */

short 
Sym_Load(char *basename)
{
#ifdef EC_EI			/* Just compile */
    extern BOOLEAN run_compiler(char *, short);
    return run_compiler(basename, 0);
#else
    char fullname[80];
    FILE *sym_fp;
    strcpy(fullname, basename);
    strcat(fullname, ".sym");
    sym_fp = fopen(fullname, "rb");
    if (sym_fp)
    {
	fread(hash, sizeof(wordpointer), MAXKEY, sym_fp);
	fread(ID_MapTable, sizeof(short), MAXIDENTIFIERS, sym_fp);
	fread(ID_NameStore, sizeof(char), MAXCHAR, sym_fp);
	fread(locspace, sizeof(char), LOC_SPACE_SIZE, sym_fp);
	fclose(sym_fp);
	return 1;		/* success */
    }
    return 0;			/* failure */
#endif
}
