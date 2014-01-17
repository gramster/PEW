/* Symbol table stuff */

#ifndef INCLUDED_SYMBOL

#include "misc.h"

#define INCLUDED_SYMBOL

/****************/
/*  Constants	*/
/****************/

#define NOIDENTIFIER	0

#define MAXCHAR         5000	/* Size of ID name store                */
#define MAXIDENTIFIERS  500	/* Maximum number of distinct id names  */

#define MAXSTRINGLENGTH	80
#define MAXCASEINDICES	256
#define LOC_SPACE_SIZE	(unsigned)(28672)
#define MAXDOMAIN	16	/* Max number of idents in single list in
				 * EXIST expression */

#define SETSIZE		4	/* 4 * 16 = 64 bits 	 */
#define MAXLEVEL	16	/* Max scope levels 	 */

/************************/
/* Process classes	*/
/************************/

#ifdef UNIX
typedef short processclass;

#define CLASS_UNATTRIB		((processclass)-1)
#define CLASS_SYSPROCESS	((processclass)0)
#define CLASS_SYSACTIVITY	((processclass)1)
#define CLASS_PROCESS		((processclass)2)
#define CLASS_ACTIVITY		((processclass)3)

#else
enum processclasses
{
    CLASS_UNATTRIB = -1, CLASS_SYSPROCESS, CLASS_SYSACTIVITY,
    CLASS_PROCESS, CLASS_ACTIVITY
};

typedef enum processclasses processclass;
#endif

/*********************/
/* Type Declarations */
/*********************/

typedef char stringtype[MAXSTRINGLENGTH];
typedef byte set[32];		/* Is this right????????????? */

enum typeclass
{
    INTEGERS, CHARS, BOOLEANS, ENUMERABLES, ENUMERATEDS, ORDINALS,
    POINTERS, RANGES, SETS, SIMPLETYPES, STRINGS
};

enum class
{
    ARRAYTYPE, BODY, CONSTANTX, CHANNELTYPE, ENUMERATEDTYPE, FIELD,
    IPTYPE, IP_, INTERACTION_, INTERACTIONARG, MODULETYPE, MODVAR,
    POINTERTYPE, PROCEDUR, RECORDTYPE, ROLETYPE, SETTYPE, STANDARDFUNC,
    STANDARDPROC, STANDARDTYPE, STATEX, STATESETX, SUBRANGETYPE,
    DERIVEDTYPE, UNDEFINED, VARIANTTYPE, VARIABLE, VALUEPARAMETER,
    VARPARAMETER
};

enum declarationstatus
{
    COMPLETE, EXTERNALX, PARTIAL, PRIMITIVE
};

typedef struct objectrecord *pointer;
typedef struct listrecord *listpointer;
typedef struct valuerecord *valuepointer;
typedef struct channelrecord *channelpointer;
typedef struct grouprecord *grouppointer;
typedef struct enumrecord *enumpointer;
typedef struct fieldrecord *fieldpointer;
typedef struct interactionrecord *interactionpointer;
typedef struct IPrecord *IPpointer;
typedef struct modulerecord *modulepointer;
typedef struct procrecord *procpointer;
typedef struct recrecord *recpointer;
typedef struct subrgrecord *subrgpointer;
typedef struct varrecord *varpointer;

struct objectrecord
{
    short ident;		/* hash key result	 */
    pointer previous;		/* for linked list	 */
    pointer type;		/* general subtype	 */
    short place;		/* index/address/label/retvaldispl */
    enum class kind;		/* entry class		 */
    union
    {
	pointer indextype;	/* class array		 */
	pointer header;		/* class body 		 */
	channelpointer _chan;	/* Class channel	 */
	valuepointer constvalue;/* class constant 	 */
	enumpointer _enum;	/* Class enumeration	 */
	fieldpointer _field;	/* Class field		 */
	interactionpointer _interact;	/* Class interaction */
	IPpointer _iptyp;	/* Class IP type	 */
	modulepointer _module;	/* Class module type	 */
	procpointer _proc;	/* Class procedure/function */
	recpointer _rec;	/* Class record		 */
	pointer statesetvalue;	/* class stateset	 */
	subrgpointer _subrg;	/* Class subrange	 */
	varpointer _var;	/* Class varaible	 */
/*                struct {
                        pointer	tagpart;
                        pointer	fieldpart;
                        listpointer	constantlist;
                        short	constants;
                        short	length;
	 *                      } _varnt;      *//* class variant	 */
    }   _cl;
};

struct channelrecord
{
    short Qtype;
    pointer user;
    pointer provider;
    grouppointer userinteractions;
    grouppointer providerinteractions;
    grouppointer bothinteractions;
};

struct grouprecord
{
    pointer interaction;
    grouppointer next;
};

struct enumrecord
{
    pointer first;
    pointer last;
};

struct fieldrecord
{
    BOOLEAN tagfield;
    BOOLEAN tagged;
    pointer selector;
    short withlevel;
};

struct interactionrecord
{
    pointer lastarg;
    pointer IPtype;
    short priority;
    short arglength;
    BOOLEAN haswhen;
    BOOLEAN hasoutput;
};

struct IPrecord
{
    pointer channel;
    pointer role;
    short discipline;
    short _delay;
};

struct labelrecord
{
    short value;
    short number;
    BOOLEAN resolved;
};

struct listrecord
{
    listpointer next;
    short value;
    short label;
};

struct modulerecord
{
    processclass processclass;
    pointer lastparameter;
    short parameterlength;
    short numips;
    short exvarlength;
    pointer lastentry;
};

struct procrecord
{
    enum declarationstatus status;
    pointer lastparam;
    short proclevel;
    short proclabel;
    BOOLEAN isfunction;
    BOOLEAN canassign;
};

struct recrecord
{
    short recordlength;
    pointer lastfield;
};

typedef unsigned short setconstanttype[SETSIZE];
typedef setconstanttype *setconstantpointer;

typedef struct stringrecord *stringpointer;
struct stringrecord
{
    pointer definedtype;
    stringtype value;
};

struct subrgrecord
{
    valuepointer lowerbound;
    valuepointer upperbound;
};

struct valuerecord
{
    pointer next;
    union
    {
	short integervalue;
	setconstantpointer setvalue;
	stringpointer stringvalue;
    }   _val;
};

struct varrecord
{
    short varlevel;
    BOOLEAN exported;
    BOOLEAN loopvariable;
};

struct blockrecord
{
    short templength;
    short templabel;
    short maxtemp;
    short varlength;
    pointer lastobject;
    struct labelrecord label;
    BOOLEAN estelleblock;
};

/*********************************************/
/* Symbol Table Variant Part : access macros */
/*********************************************/

#define _Aray_IxType	_cl.indextype
#define _Aray_RangType	_Aray_IxType->type

#define _Body_Header	_cl.header

#define _Chan		_cl._chan
#define _Chan_QType	_Chan->Qtype
#define _Chan_User	_Chan->user
#define _Chan_Prov	_Chan->provider
#define _Chan_UInts	_Chan->userinteractions
#define _Chan_PInts	_Chan->providerinteractions
#define _Chan_UPInts	_Chan->bothinteractions

#define _Const_Val	_cl.constvalue

#define _Enum		_cl._enum
#define _Enum_First	_Enum->first
#define _Enum_Last	_Enum->last

#define _Fld		_cl._field
#define _Fld_IsTag	_Fld->tagfield
#define _Fld_IsTagd	_Fld->tagged
#define _Fld_Selctr	_Fld->selector
#define _Fld_WithLevel	_Fld->withlevel

#define _Intr		_cl._interact
#define _Intr_IPType	_Intr->IPtype
#define _Intr_Priority	_Intr->priority
#define _Intr_LastArg	_Intr->lastarg
#define _Intr_ArgLen	_Intr->arglength
#define _Intr_HasWhen	_Intr->haswhen
#define _Intr_HasOutput	_Intr->hasoutput

#define _IPTyp		_cl._iptyp
#define _IPTyp_Delay	_IPTyp->_delay
#define _IPTyp_Chan	_IPTyp->channel
#define _IPTyp_Role	_IPTyp->role
#define _IPTyp_Disc	_IPTyp->discipline

#define _Mod		_cl._module
#define _Mod_Class	_Mod->processclass
#define _Mod_LastPar	_Mod->lastparameter
#define _Mod_ParmLen	_Mod->parameterlength
#define _Mod_ExVars	_Mod->exvarlength
#define _Mod_NumIPs	_Mod->numips
#define _Mod_LastEntry	_Mod->lastentry

#define _Proc		_cl._proc
#define _Proc_Status	_Proc->status
#define _Proc_LastPar	_Proc->lastparam
#define _Proc_Level	_Proc->proclevel
#define _Proc_Label	_Proc->proclabel
#define _Proc_IsFunc	_Proc->isfunction
#define _Proc_CanAsgn	_Proc->canassign

#define _Rec		_cl._rec
#define _Rec_Len	_Rec->recordlength
#define _Rec_LastFld	_Rec->lastfield

#define _StSet_Val	_cl.statesetvalue

#define _Sub		_cl._subrg
#define _Sub_Lower	_Sub->lowerbound
#define _Sub_LowerVal	_Sub_Lower->_Val_Int
#define _Sub_Upper	_Sub->upperbound
#define _Sub_UpperVal	_Sub_Upper->_Val_Int

#define _Val_Int	_val.integervalue
#define _Val_Set	_val.setvalue
#define _Val_Str	_val.stringvalue
#define _Val_String	_Val_Str->value

#define _Const_Int	_Const_Val->_Val_Int
#define _Const_Set	_Const_Val->_Val_Set
#define _Next_Const	_Const_Val->next

/*
#define _Vrnt_Tag	_cl._varnt.tagpart
#define _Vrnt_Field	_cl._varnt.fieldpart
#define _Vrnt_CList	_cl._varnt.constantlist
#define _Vrnt_Consts	_cl._varnt.constants
#define _Vrnt_Len	_cl._varnt.length
*/
#define _Var		_cl._var
#define _Var_Level	_Var->varlevel
#define _Var_IsExp	_Var->exported
#define _Var_IsLoop	_Var->loopvariable

#define _Aray_Lower	_Aray_IxType->_Sub_Lower
#define _Aray_LowerVal	_Aray_Lower->_val.integervalue
#define _Aray_Upper	_Aray_IxType->_Sub_Upper
#define _Aray_UpperVal	_Aray_Upper->_val.integervalue

/**********************************************/
/* Structures for generating analytical model */
/**********************************************/
/*
typedef struct EventDetails
{
  short eventID;
  short server;
  struct EventList *eventlist;
} EventDetails;

typedef struct EventList
{
  struct EventDetails *event;
  struct trans_entry  *trans;
  struct EventList    *next;
} EventList;
*/

/**************/
/* Prototypes */
/**************/
char *ClassName(processclass c, BOOLEAN shortform);
char *EC_malloc(short nbytes);
void Sym_Initialise(void);
void insert(BOOLEAN is_ident, STRING inserttext, short IDindex);
short findID(stringtype searchtext);	/* Get index given name */
short search(stringtype searchtext, BOOLEAN * is_ident);
pointer find_definition(short ident);
pointer define(short ident, enum class kind);
void get_symbolic_name(short ident, ushort index, char *name, char *zero_val, short bufsize);
void Get_TransName();		/* No prototype as we would have to include
				 * interp.h */
void Get_TransStatusLine(ushort tr_num, char *buff, BOOLEAN ForDisplay);
void Get_TransStatLine(char *buff, ushort tr_num, BOOLEAN ForDisplay);
void Get_IPStatLine(char *buff, ushort ip_num, BOOLEAN dummy);
void Sym_Save(char *basename);
short Sym_Load(char *basename);

/*****************************************************/
/* Variables global to both compiler and interpreter */
/*****************************************************/

#ifndef GLOBAL
#define GLOBAL extern
#endif

GLOBAL char *locspace, *locspacenow;
GLOBAL struct blockrecord block[MAXLEVEL + 1];
GLOBAL int blocklevel, out_address, in_address, variable_scope_base;
GLOBAL short idents;
GLOBAL short ID_MapTable[MAXIDENTIFIERS];
GLOBAL char ID_NameStore[MAXCHAR];

#endif				/* INCLUDED_SYMBOL */
