#define NEED_OP_ARGS
#define IS_MAIN
#include <time.h>
#include "symbol.h"
#undef GLOBAL
#define GLOBAL
#include "compiler.h"
#undef GLOBAL

/*************/
/* Constants */
/*************/

#define MAXINT		32767	/* Largest integer value		 */
#define MAXKEY		631	/* Number of hash table entries		 */
#define BITSPERWORD	16	/* Used for set size calculations	 */

/*******************/
/* Macro functions */
/*******************/

#define inrange(v,l,u,k) 	if ((v)<(l)||(v)>(u)) compiler_error(900,k)

/*********************************/
/* Prototypes of local functions */
/*********************************/

void endline(void);
void nextchar(void);

/***************/
/* Global vars */
/***************/

#if 0
BOOLEAN emitcode, fast_write;
short
    tgroup, labelno, argument, lineno, default_reliability, dist_val,	/* Expected value	 */
    distribution_type, chanQtype,	/* Channnel queueing type */
    chanQpri,			/* Priority for priority Q */
    IPQdelay;			/* IP Propogation delay	 */
enum symboltype symbol;
pointer typeuniversal, typeinteger, typechar, typeboolean, timeunit;
char ch, ec_buffer[256];
ushort *emitbuff;		/* code buffer */
short ec_errline;
ecode_op op;
FILE *sourcefile, *targetfile;
#endif


static void 
check_interactions(grouppointer igroup)
{
    while (igroup)
    {
	pointer tmp = igroup->interaction;
	if (tmp->_Intr_HasWhen && !tmp->_Intr_HasOutput)
	    warn(WARN_WHENOUT, tmp->ident);
	else if (tmp->_Intr_HasOutput && !tmp->_Intr_HasWhen)
	    warn(WARN_OUTWHEN, tmp->ident);
	else if (!tmp->_Intr_HasWhen && !tmp->_Intr_HasOutput)
	    warn(WARN_UNREFINTR, tmp->ident);
	igroup = igroup->next;
    }
}

void 
exitblock(void)
{
    pointer tmp;
#ifndef INTEGRATE
    extern BOOLEAN dump_stab;
    if (dump_stab)
    {
	pointer obj = block[blocklevel].lastobject;
	printf("\n\n\n\nExit block at %d (level %d)\n\n",
	       (int)(PTR_TO_LONG(obj) - PTR_TO_LONG(locspace)), blocklevel);
	print_stab_level(blocklevel);
    }
#endif
    tmp = block[blocklevel].lastobject;
    while (tmp != NULL)
    {
	if (tmp->kind == CHANNELTYPE)
	{
	    check_interactions(tmp->_Chan_UInts);
	    check_interactions(tmp->_Chan_PInts);
	    check_interactions(tmp->_Chan_UPInts);
	}
	tmp = tmp->previous;
    }
    emit(OP_DEFARG, block[blocklevel].templabel, block[blocklevel].maxtemp);
    blocklevel--;
}

/*******************************/
/* Temporary variable handling */
/*******************************/

void 
push(short length)
{
    block[blocklevel].templength += length;
    if (block[blocklevel].maxtemp < block[blocklevel].templength)
	block[blocklevel].maxtemp = block[blocklevel].templength;
}

void 
pop(short length)
{
    block[blocklevel].templength -= length;
}

/********************/
/* Lexical Analysis */
/********************/

static short NEAR 
get_value(void)
{
    short rtn = 0;
    nextchar();			/* Skip directive character */
    if (ch >= '0' && ch <= '9')
	while (ch >= '0' && ch <= '9')
	{
	    rtn = rtn * 10 + ch - '0';
	    nextchar();
	}
    return rtn;
}

static void NEAR 
compiler_directive(void)
{
    /*
     * Directives are: $P<val>	- delays are Poisson, with expected value
     * <val> $E<val>	- delays are exponential, with expected value <val>
     * $U	- delays are uniform $G<val>	- delays are geometric, with
     * expected value <val> $R<val>	- set default reliability to <val>
     * percent. $O<val>	- set output options. $F[+|-]	- fast write; no
     * pause on endwrite $T	- indicate a split in transition grouping for
     * the transition sequence table
     * 
     * $QR	- use random queueing for next channel type $QP	- use priority
     * queueing for next channel type $S<val> - set priority $D<val> - set
     * propogation delay for next interaction point declaration statement
     */
    nextchar();
    if (ch >= 'a' && ch <= 'z')
	ch -= ('a' - 'A');	/* make upper case */
    switch (ch)
    {
    case 'R':
	{
	    /* Channel reliability */
	    short reliability = get_value();
	    if (reliability > 0 && reliability <= 100)
		default_reliability = reliability;
	}
	break;
    case 'O':			/* Output directive */
	nextchar();
	emit(OP_SETOUTPUT, ch - '0');
	break;
    case 'U':			/* Uniformly distributed delays */
	distribution_type = 0;
	break;
    case 'P':			/* Delays are Poisson-distributed */
	distribution_type = 1;
	dist_val = get_value();
	break;
    case 'E':			/* Delays are exponentially distributed */
	distribution_type = 2;
	dist_val = get_value();
	break;
    case 'G':			/* Delays are geometrically distributed */
	distribution_type = 3;
	dist_val = get_value();
	break;
    case 'F':			/* fast-write enable/disable */
	nextchar();
	if (ch == '+')
	    fast_write = TRUE;
	else if (ch == '-')
	    fast_write = FALSE;
	break;
    case 'T':
	tgroup = 99;		/* Tells parser we must set this */
	break;
    case 'Q':
	nextchar();
	chanQpri = 0;
	if (ch == 'R')
	    chanQtype = RANDOM_Q;
	else if (ch == 'P')
	{
	    chanQtype = PRIORITY_Q;
	}
	break;
    case 'S':			/* Set priority */
	chanQpri = get_value();
	break;
    case 'D':			/* Propogation delay */
	IPQdelay = get_value();
	break;
    }
}


static void NEAR 
comment(void)
{
    while (ch != '}' && ch != ETX)
    {
	if (ch == '*')
	{
	    nextchar();
	    if (ch == ')')
		break;
	} else if (ch == '\n')
	    endline();
	else if (ch == '$')
	    compiler_directive();
	else
	    nextchar();
    }
    if (ch != ETX)
	nextchar();
    else
	compiler_error(1, ERR_COMMENT);
}


void 
_nextsymbol(void)
{
    BOOLEAN is_ident;
    short length, IDindex, tempvalue, digit;
    char *tokenstring = ec_buffer;

    symbol = SYM_UNDEF;
    *tokenstring = '\0';
    while (symbol == SYM_UNDEF)
    {
	switch (ch)
	{
	case ETX:
	    symbol = SYM_ENDTEXT;
	    break;
	case '\t':
	case ' ':
	    nextchar();
	    break;
	case '\n':
	    endline();
	    break;
	case '{':
	    comment();
	    break;
	case '(':
	    nextchar();
	    if (ch == '.')
	    {
		symbol = SYM_LEFTBRACKET;
		nextchar();
	    } else if (ch == '*')
		comment();
	    else
		symbol = SYM_LEFTPARENTHESIS;
	    break;
	case '\'':
	    nextchar();
	    if (ch >= 32 && ch <= 126 && ch != '\'')
	    {
		symbol = SYM_GRAPHIC;
		*tokenstring++ = ch;
		nextchar();
		while (ch >= 32 && ch <= 126 && ch != '\'')
		{
		    *tokenstring++ = ch;
		    nextchar();
		}
		if (ch == '\'')
		    nextchar();
		else
		    compiler_error(2, ERR_STRING);
	    } else if (ch == '\'')
		nextchar();
	    else
		compiler_error(3, ERR_STRING);
	    *tokenstring = '\0';
	    break;
	case ':':
	    nextchar();
	    if (ch == '=')
	    {
		symbol = SYM_BECOMES;
		nextchar();
	    } else
		symbol = SYM_COLON;
	    break;
	case '.':
	    nextchar();
	    if (ch == '.')
	    {
		nextchar();
		if (ch == '.')
		{
		    symbol = SYM_TRIPLEDOT;
		    nextchar();
		} else
		    symbol = SYM_DOUBLEDOT;
	    } else if (ch == ')')
	    {
		symbol = SYM_RIGHTBRACKET;
		nextchar();
	    } else
		symbol = SYM_PERIOD;
	    break;
	case '<':
	    nextchar();
	    if (ch == '=')
	    {
		symbol = SYM_NOTGREATER;
		nextchar();
	    } else if (ch == '>')
	    {
		symbol = SYM_NOTEQUAL;
		nextchar();
	    } else
		symbol = SYM_LESS;
	    break;
	case '>':
	    nextchar();
	    if (ch == '=')
	    {
		symbol = SYM_NOTLESS;
		nextchar();
	    } else
		symbol = SYM_GREATER;
	    break;
	case '+':
	    symbol = SYM_PLUS;
	    nextchar();
	    break;
	case '-':
	    symbol = SYM_MINUS;
	    nextchar();
	    break;
	case '*':
	    symbol = SYM_ASTERISK;
	    nextchar();
	    break;
	case '=':
	    symbol = SYM_EQUAL;
	    nextchar();
	    break;
	case ')':
	    symbol = SYM_RIGHTPARENTHESIS;
	    nextchar();
	    break;
	case '[':
	    symbol = SYM_LEFTBRACKET;
	    nextchar();
	    break;
	case ']':
	    symbol = SYM_RIGHTBRACKET;
	    nextchar();
	    break;
	case ',':
	    symbol = SYM_COMMA;
	    nextchar();
	    break;
	case ';':
	    symbol = SYM_SEMICOLON;
	    nextchar();
	    break;
	case '@':
	case '^':
	    symbol = SYM_HAT;
	    nextchar();
	    break;
	default:
	    if (ch >= '0' && ch <= '9')
	    {
		tempvalue = 0;
		while (isdigit(ch))
		{
		    digit = ch - '0';
		    if (tempvalue <= (MAXINT - digit) / 10)
		    {
			tempvalue = 10 * tempvalue + digit;
			nextchar();
		    } else
		    {
			compiler_error(4, ERR_INTEGER);
			while (isdigit(ch))
			    nextchar();
		    }
		}
		symbol = SYM_INTEGER;
		argument = tempvalue;
	    } else if (isalpha(ch) || ch == '_')
	    {
		length = 0;
		while (isalnum(ch) || ch == '_')
		{
		    if (isupper(ch))
			ch = (char)tolower(ch);
		    ec_buffer[length++] = ch;
		    nextchar();
		}
		ec_buffer[length] = '\0';
		IDindex = search(ec_buffer, &is_ident);
		if (is_ident)
		{
		    symbol = SYM_IDENTIFIER;
		    argument = IDindex;
		} else
		    symbol = (enum symboltype)IDindex;
	    } else
	    {
		symbol = SYM_UNKNOWN;
		nextchar();
	    }
	    break;
	}

    }
}

void 
    expect(enum symboltype s)
    {
	if (symbol == s)
	    nextsymbol();
	else
	{
	    register enum errornumber e;
	    switch (s)
	    {
		case SYM_SEMICOLON:e = ERR_NOSEMICOLON;
		break;
		case SYM_COLON:e = ERR_NOCOLON;
		break;
		case SYM_RIGHTPARENTHESIS:e = ERR_NORIGHTPAR;
		break;
		case SYM_IDENTIFIER:e = ERR_NOIDENT;
		break;
		case SYM_LEFTPARENTHESIS:e = ERR_NOLEFTPAR;
		break;
		case SYM_COMMA:e = ERR_NOCOMMA;
		break;
		case SYM_END:e = ERR_NOEND;
		break;
		case SYM_DO:e = ERR_NODO;
		break;
		case SYM_LEFTBRACKET:e = ERR_NOLEFTBRAK;
		break;
		case SYM_PERIOD:e = ERR_NOPERIOD;
		break;
		case SYM_RIGHTBRACKET:e = ERR_NORIGHTBRAK;
		break;
		case SYM_OF:e = ERR_NOOF;
		break;
		default:e = ERR_SYNTAX;
	    }
	        compiler_error(5, e);
	}
    }

short 
expect_ident(void)
{
    short ident = argument;
    if (symbol == SYM_IDENTIFIER)
	nextsymbol();
    else
	compiler_error(5, ERR_NOIDENT);
    return ident;
}

/*************************************/
/* External Scope Level entry points */
/*************************************/

void 
new_block(BOOLEAN estelleblock)
{
    struct blockrecord *current = &block[++blocklevel];
    inrange(blocklevel, 0, MAXLEVEL - 1, ERR_LEVEL);
    current->estelleblock = estelleblock;
    current->templength = 0;
    current->templabel = new_label();
    current->varlength = 0;
    current->maxtemp = 0;
    current->lastobject = NULL;
    current->label.value = 0;
    current->label.number = 0;
    emit(OP_ENTERBLOCK, current->templabel);
}


/**************************************/
/* External Symbol Table entry points */
/**************************************/


pointer 
    define_nextident(enum class kind)
    {
	short ident = argument;
	if  (symbol == SYM_IDENTIFIER)
	        nextsymbol();
	else
	        compiler_error(5, ERR_NOIDENT);
	    return define(ident, kind);
    }



    pointer find(short ident)
{
    pointer object = find_definition(ident);
    if (object == NULL)
	undef_ident_err(7, ID_NameStore + ID_MapTable[ident]);
    return object;
}

void 
redefine(pointer object)
{
    pointer newobject, newprevious;
    newobject = define(NOIDENTIFIER, UNDEFINED);
    newprevious = newobject->previous;
    *newobject = *object;
    newobject->previous = newprevious;
}

/******************************
	TYPE ANALYSIS
*******************************/

BOOLEAN 
    isin(pointer typex, enum typeclass class)
    {
	BOOLEAN rtn = FALSE;
	switch (class)
	{
	    case BOOLEANS:if (typex == typeboolean)
		rtn = TRUE;
	    break;
	    case CHARS:if (typex == typechar)
		rtn = TRUE;
	    break;
	    case INTEGERS:if (typex == typeinteger)
		rtn = TRUE;
	    break;
	    case SIMPLETYPES:
	    case ORDINALS:if (typex == typechar)
		rtn = TRUE;
	    case ENUMERABLES:if ((typex == typeinteger) || (typex->kind == SUBRANGETYPE))
		rtn = TRUE;
	    case ENUMERATEDS:if (typex->kind == ENUMERATEDTYPE)
		rtn = TRUE;
	    break;
	    case POINTERS:if (typex->kind == POINTERTYPE)
		rtn = TRUE;
	    break;
	    case RANGES:if (typex->kind == SUBRANGETYPE)
		rtn = TRUE;
	    break;
	    case SETS:rtn = (BOOLEAN) (typex->kind == SETTYPE);
	    break;
	    case STRINGS:if (typex->kind == ARRAYTYPE)
		rtn = (BOOLEAN) (isin(typex->_Aray_RangType, ORDINALS)
				 && isin(typex->type, CHARS));
	    else if (typex->kind == CONSTANTX)
		rtn = isin(typex->type, STRINGS);
	    break;
	}
	    return rtn;
    }


BOOLEAN 
samestringtype(pointer typex1, pointer typex2)
{
    if (isin(typex1, STRINGS) && isin(typex2, STRINGS))
    {
	pointer temptype1, temptype2;
	int rangelength1, rangelength2;
	if (typex1->kind == CONSTANTX)
	    temptype1 = typex1->type;
	else
	    temptype1 = typex1;
	temptype1 = temptype1->_Aray_IxType;
	if (typex2->kind == CONSTANTX)
	    temptype2 = typex2->type;
	else
	    temptype2 = typex2;
	temptype2 = temptype2->_Aray_IxType;
	rangelength1 = temptype1->_Sub_UpperVal - temptype1->_Sub_LowerVal;
	rangelength2 = temptype2->_Sub_UpperVal - temptype2->_Sub_LowerVal;
	return (BOOLEAN) (rangelength1 == rangelength2);
    }
    return FALSE;
}


enum compatibility_class 
compatibletypes(pointer t1, pointer t2)
{
    enum compatibility_class rtn = INCOMPATIBLE;
    if ((t1 == typeuniversal) || (t2 == typeuniversal))
	rtn = ANYCOMPATIBLE;
    else if ((isin(t1, SETS) && isin(t2, SETS)))
	if (t1->type == t2->type || t2->type == typeuniversal)
	    rtn = SETCOMPATIBLE;
	else
	    compiler_error(8, ERR_COMPATIBLE);
    else if (t1 == t2)
	rtn = COMPATIBLE;
    else if (t1->kind == POINTERTYPE && t2->kind == POINTERTYPE)
    {
	if ((t1->type == t2->type) || (t2->type == typeuniversal) ||
	    (t1->type == typeuniversal))
	    rtn = COMPATIBLE;
    } else if (samestringtype(t1, t2))
	rtn = STRINGCOMPATIBLE;
    else if (isin(t2, RANGES))
	if (t2->type == t1)
	    rtn = SUBRANGECOMPATIBLE;
	else if (isin(t1, RANGES))
	    if (t1->type == t2->type)
		rtn = RANGECOMPATIBLE;
	    else
		compiler_error(9, ERR_COMPATIBLE);
	else
	    compiler_error(10, ERR_COMPATIBLE);
    else if (isin(t1, RANGES))
	if (t1->type == t2)
	    rtn = RANGECOMPATIBLE;	/* This was 5 */
	else
	    compiler_error(11, ERR_COMPATIBLE);
    else if (isin(t1, ORDINALS) && isin(t2, SETS))
	if (t1 == t2->type)
	    rtn = ELTOFCOMPATIBLE;
	else
	    compiler_error(12, ERR_COMPATIBLE);
    else
	compiler_error(13, ERR_COMPATIBLE);
    return rtn;
}

short 
typelength(pointer typex)
{
    short rtn = 0, tmp, tmp1;
    switch (typex->kind)
    {
    case ARRAYTYPE:
	rtn = (typex->_Aray_IxType->_Sub_UpperVal
	       - typex->_Aray_IxType->_Sub_LowerVal + 1);
	rtn *= typelength(typex->type);
	break;
    case RECORDTYPE:
	rtn = typex->_Rec_Len;
	break;
    case SUBRANGETYPE:
	rtn = typelength(typex->type);
	break;
    case SETTYPE:
	rtn = SETSIZE;
	if (typex->type->kind == ENUMERATEDTYPE)
	{
	    tmp = typex->type->_Enum_Last->_Const_Val->_Val_Int;
	    inrange(tmp, 0, SETSIZE * BITSPERWORD - 1, ERR_SET);
	} else if (typex->type->kind == SUBRANGETYPE)
	{
	    tmp = typex->type->_Sub_UpperVal;
	    tmp1 = typex->type->_Sub_LowerVal;
	    inrange(tmp, 0, SETSIZE * BITSPERWORD - 1, ERR_SET);
	    inrange(tmp1, 0, SETSIZE * BITSPERWORD - 1, ERR_SET);
	} else if (typex->type != typeuniversal)	/* JAque had boolean */
	    compiler_error(17, ERR_SET);
	break;
    case DERIVEDTYPE:
	rtn = typelength(typex->type);
	break;
    case FIELD:
	rtn = typelength(typex->type);
	break;
    case ENUMERATEDTYPE:
    case IPTYPE:
    case STANDARDTYPE:
    case POINTERTYPE:
    case MODULETYPE:
	rtn = 1;
	break;
    default:
	compiler_error(18, ERR_TYPE);
	break;
    }
    return rtn;
}

/*********************************************************************/

static void NEAR 
define_proc(short ident)
{
    pointer rtn = define(ident, STANDARDPROC);
    rtn->_Proc = (procpointer) EC_malloc(sizeof(struct procrecord));
    rtn->_Proc_CanAsgn = FALSE;
    rtn->_Proc_IsFunc = FALSE;
}

static void NEAR 
define_func(short ident)
{
    pointer rtn = define(ident, STANDARDFUNC);
    rtn->_Proc = (procpointer) EC_malloc(sizeof(struct procrecord));
    rtn->_Proc_CanAsgn = FALSE;
    rtn->_Proc_IsFunc = TRUE;
}


pointer 
define_const_int(short ident, pointer type, short val)
{
    pointer rtn = define(ident, CONSTANTX);
    rtn->_Const_Val = (valuepointer) EC_malloc(sizeof(struct valuerecord));
    rtn->type = type;
    rtn->_Const_Val->_Val_Int = val;
    return rtn;
}

static void NEAR 
standardblock(char *Anz_Name, short Anz_Ident, short Anz_Value)
{
    pointer typex;

    blocklevel = -1;
    new_block(TRUE);
    typeuniversal = define(NOIDENTIFIER, STANDARDTYPE);
    typeinteger = define(_INTEGER, STANDARDTYPE);
    typechar = define(_CHAR, STANDARDTYPE);
    typeboolean = define(_BOOLEAN, STANDARDTYPE);
    typeboolean->kind = ENUMERATEDTYPE;
    typeboolean->_Enum = (enumpointer) EC_malloc(sizeof(struct enumrecord));
    typeboolean->_Enum_Last = define_const_int(_TRUE, typeboolean, (int)TRUE);
    typeboolean->_Enum_First = define_const_int(_FALSE, typeboolean, (int)FALSE);
    typeboolean->_Enum_Last->_Const_Val->next = NULL;
    typeboolean->_Enum_First->_Const_Val->next = typeboolean->_Enum_Last;

    typex = define(NOIDENTIFIER, POINTERTYPE);
    typex->type = typeuniversal;
     /*	nilpointer = */ (void)define_const_int(_NIL, typex, 0);

/****
	emptyset = define(NOIDENTIFIER,CONSTANTX);
	emptyset->_Const_Val = (valuepointer)EC_malloc(sizeof(struct valuerecord));
	for (counter=0; counter<SETSIZE; counter++) nullset[counter] = 0;
	emptyset->_Const_Val->_val.setvalue = (setconstantpointer)nullset;
	emptyset->type = typeuniversal;
****/

    timeunit = define(NOIDENTIFIER, UNDEFINED);
    (void)define_const_int(_HOURS, timeunit, 0);
    (void)define_const_int(_MINUTES, timeunit, 1);
    (void)define_const_int(_SECONDS, timeunit, 2);
    (void)define_const_int(_MILLISECONDS, timeunit, 3);
    (void)define_const_int(_MICROSECONDS, timeunit, 4);
    (void)define_const_int(_SIZEOF, typeinteger, 0);	/* Pseudo-function */
    if (Anz_Name != NULL)
	(void)define_const_int(Anz_Ident, typeinteger, Anz_Value);
    define_func(_ABS);
    define_func(_SQR);
    define_func(_ORD);
    define_func(_CHR);
    define_func(_SUCC);
    define_func(_PRED);
    define_func(_ODD);
    define_func(_RANDOM);
    define_func(_ERANDOM);	/* Poisson distributed random numbers */
    define_func(_PRANDOM);	/* -ve exponential distributed random numbers */
    define_func(_GRANDOM);	/* Geometric */
    define_func(_QLENGTH);
    define_func(_FIRECOUNT);
    define_func(_GLOBALTIME);
    define_proc(_UNPACK);
    define_proc(_PACK);
    define_proc(_NEW);
    define_proc(_DISPOSE);
    define_proc(_READ);
    define_proc(_WRITE);
    define_proc(_WRITELN);
}

#define DEFINE(isid,txt,ix) insert((BOOLEAN)isid,(STRING)txt,(int)ix);

void 
compile(char *Anz_Name, short Anz_Value)
{
    lineno = 0;
    default_reliability = 100;
    ch = '\n';
    labelno = 0;
    chanQtype = FIFO_Q;
    chanQpri = 0;
    IPQdelay = 0;
    Sym_Initialise();
    fast_write = FALSE;
    idents = 0;
    DEFINE(FALSE, "activity", SYM_ACTIVITY);
    DEFINE(FALSE, "all", SYM_ALL);
    DEFINE(FALSE, "and", SYM_AND);
    DEFINE(FALSE, "any", SYM_ANY);
    DEFINE(FALSE, "array", SYM_ARRAY);
    DEFINE(FALSE, "attach", SYM_ATTACH);
    DEFINE(FALSE, "begin", SYM_BEGIN);
    DEFINE(FALSE, "body", SYM_BODY);
    DEFINE(FALSE, "by", SYM_BY);
    DEFINE(FALSE, "case", SYM_CASE);
    DEFINE(FALSE, "channel", SYM_CHANNEL);
    DEFINE(FALSE, "common", SYM_COMMON);
    DEFINE(FALSE, "connect", SYM_CONNECT);
    DEFINE(FALSE, "const", SYM_CONST);
    DEFINE(FALSE, "default", SYM_DEFAULT);
    DEFINE(FALSE, "delay", SYM_DELAY);
    DEFINE(FALSE, "detach", SYM_DETACH);
    DEFINE(FALSE, "disconnect", SYM_DISCONNECT);
    DEFINE(FALSE, "div", SYM_DIV);
    DEFINE(FALSE, "do", SYM_DO);
    DEFINE(FALSE, "downto", SYM_DOWNTO);
    DEFINE(FALSE, "else", SYM_ELSE);
    DEFINE(FALSE, "end", SYM_END);
    DEFINE(FALSE, "exist", SYM_EXIST);
    DEFINE(FALSE, "export", SYM_EXPORT);
    DEFINE(FALSE, "external", SYM_EXTERNAL);
    DEFINE(FALSE, "for", SYM_FOR);
    DEFINE(FALSE, "forone", SYM_FORONE);
    DEFINE(FALSE, "forward", SYM_FORWARD);
    DEFINE(FALSE, "from", SYM_FROM);
    DEFINE(FALSE, "function", SYM_FUNCTION);
    DEFINE(FALSE, "goto", SYM_GOTO);
    DEFINE(FALSE, "if", SYM_IF);
    DEFINE(FALSE, "in", SYM_IN);
    DEFINE(FALSE, "individual", SYM_INDIVIDUAL);
    DEFINE(FALSE, "init", SYM_INIT);
    DEFINE(FALSE, "initialize", SYM_INITIALIZE);
    DEFINE(FALSE, "ip", SYM_IP);
    DEFINE(FALSE, "label", SYM_LABEL);
    DEFINE(FALSE, "mod", SYM_MOD);
    DEFINE(FALSE, "module", SYM_MODULE);
    DEFINE(FALSE, "modvar", SYM_MODVAR);
    DEFINE(FALSE, "name", SYM_NAME);
    DEFINE(FALSE, "not", SYM_NOT);
    DEFINE(FALSE, "of", SYM_OF);
    DEFINE(FALSE, "or", SYM_OR);
    DEFINE(FALSE, "otherwise", SYM_OTHERWISE);
    DEFINE(FALSE, "output", SYM_OUTPUT);
    DEFINE(FALSE, "packed", SYM_PACKED);
    DEFINE(FALSE, "primitive", SYM_PRIMITIVE);
    DEFINE(FALSE, "priority", SYM_PRIORITY);
    DEFINE(FALSE, "procedure", SYM_PROCEDURE);
    DEFINE(FALSE, "provided", SYM_PROVIDED);
    DEFINE(FALSE, "process", SYM_PROCESS);
    DEFINE(FALSE, "pure", SYM_PURE);
    DEFINE(FALSE, "queue", SYM_QUEUE);
    DEFINE(FALSE, "record", SYM_RECORD);
    DEFINE(FALSE, "release", SYM_RELEASE);
    DEFINE(FALSE, "repeat", SYM_REPEAT);
    DEFINE(FALSE, "same", SYM_SAME);
    DEFINE(FALSE, "set", SYM_SET);
    DEFINE(FALSE, "specification", SYM_SPECIFICATION);
    DEFINE(FALSE, "state", SYM_STATE);
    DEFINE(FALSE, "stateset", SYM_STATESET);
    DEFINE(FALSE, "suchthat", SYM_SUCHTHAT);
    DEFINE(FALSE, "systemactivity", SYM_SYSTEMACTIVITY);
    DEFINE(FALSE, "systemprocess", SYM_SYSTEMPROCESS);
    DEFINE(FALSE, "terminate", SYM_TERMINATE);
    DEFINE(FALSE, "then", SYM_THEN);
    DEFINE(FALSE, "timescale", SYM_TIMESCALE);
    DEFINE(FALSE, "to", SYM_TO);
    DEFINE(FALSE, "trans", SYM_TRANS);
    DEFINE(FALSE, "type", SYM_TYPE);
    DEFINE(FALSE, "until", SYM_UNTIL);
    DEFINE(FALSE, "var", SYM_VAR);
    DEFINE(FALSE, "when", SYM_WHEN);
    DEFINE(FALSE, "while", SYM_WHILE);
    DEFINE(FALSE, "with", SYM_WITH);
    DEFINE(TRUE, "integer", _INTEGER);
    DEFINE(TRUE, "boolean", _BOOLEAN);
    DEFINE(TRUE, "char", _CHAR);
    DEFINE(TRUE, "false", _FALSE);
    DEFINE(TRUE, "true", _TRUE);
    DEFINE(TRUE, "nil", _NIL);
    DEFINE(TRUE, "hours", _HOURS);
    DEFINE(TRUE, "microseconds", _MICROSECONDS);
    DEFINE(TRUE, "milliseconds", _MILLISECONDS);
    DEFINE(TRUE, "minutes", _MINUTES);
    DEFINE(TRUE, "seconds", _SECONDS);
    DEFINE(TRUE, "unpack", _UNPACK);
    DEFINE(TRUE, "pack", _PACK);
    DEFINE(TRUE, "new", _NEW);
    DEFINE(TRUE, "dispose", _DISPOSE);
    DEFINE(TRUE, "abs", _ABS);
    DEFINE(TRUE, "sqr", _SQR);
    DEFINE(TRUE, "ord", _ORD);
    DEFINE(TRUE, "chr", _CHR);
    DEFINE(TRUE, "succ", _SUCC);
    DEFINE(TRUE, "pred", _PRED);
    DEFINE(TRUE, "odd", _ODD);
    DEFINE(TRUE, "read", _READ);
    DEFINE(TRUE, "write", _WRITE);
    DEFINE(TRUE, "writeln", _WRITELN);
    DEFINE(TRUE, "random", _RANDOM);
    DEFINE(TRUE, "erandom", _ERANDOM);
    DEFINE(TRUE, "prandom", _PRANDOM);
    DEFINE(TRUE, "grandom", _GRANDOM);
    DEFINE(TRUE, "qlength", _QLENGTH);
    DEFINE(TRUE, "firecount", _FIRECOUNT);
    DEFINE(TRUE, "globaltime", _GLOBALTIME);
    DEFINE(TRUE, "sizeof", _SIZEOF);
    idents = FIRSTUSERIDENTIFIER;
    if (Anz_Name != NULL)
    {
	DEFINE(TRUE, Anz_Name, idents++);
    }
    endline();
    nextsymbol();
    standardblock(Anz_Name, idents - 1, Anz_Value);
    specification();
    exitblock();
}
