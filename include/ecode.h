/* ecode.h */

#ifndef INCLUDED_ECODE

#define INCLUDED_ECODE

extern BOOLEAN emitcode;
extern short address;

#ifdef UNIX			/* enums are 4 bytes in Unix, which we don't
				 * want */

typedef ushort ecode_op;

#define	OP_ADD			((ecode_op)0)
#define	OP_ADDRESS		((ecode_op)1)
#define	OP_ADDSETELEMENT	((ecode_op)2)
#define	OP_ADDSETRANGE		((ecode_op)3)
#define	OP_AND			((ecode_op)4)
#define	OP_ASSIGN		((ecode_op)5)
#define	OP_ATTACH		((ecode_op)6)
#define	OP_CASE			((ecode_op)7)
#define	OP_CONNECT		((ecode_op)8)
#define	OP_CONSTANT		((ecode_op)9)
#define	OP_COPY			((ecode_op)10)
#define	OP_DETACH		((ecode_op)11)
#define	OP_DIFFERENCE		((ecode_op)12)
#define	OP_DISCONNECT		((ecode_op)13)
#define	OP_DIV			((ecode_op)14)
#define	OP_DO			((ecode_op)15)
#define	OP_ENDPROC		((ecode_op)16)
#define	OP_ENDTRANS		((ecode_op)17)
#define	OP_EQUAL		((ecode_op)18)
#define	OP_FIELD		((ecode_op)19)
#define	OP_FOR			((ecode_op)20)
#define	OP_GOTO			((ecode_op)21)
#define	OP_GREATER		((ecode_op)22)
#define	OP_IN			((ecode_op)23)
#define	OP_INTERSECTION		((ecode_op)24)
#define	OP_INDEX		((ecode_op)25)
#define	OP_INDEXEDJUMP		((ecode_op)26)
#define	OP_INIT			((ecode_op)27)
#define	OP_LESS			((ecode_op)28)
#define	OP_MINUS		((ecode_op)29)
#define	OP_MODULEASSIGN		((ecode_op)30)
#define	OP_MODULO		((ecode_op)31)
#define	OP_MULTIPLY		((ecode_op)32)
#define	OP_NEWLINE		((ecode_op)33)
#define	OP_NEXT			((ecode_op)34)
#define	OP_NEXTINSTANCE		((ecode_op)35)
#define	OP_NOT			((ecode_op)36)
#define	OP_NOTEQUAL		((ecode_op)37)
#define	OP_NOTGREATER		((ecode_op)38)
#define	OP_NOTLESS		((ecode_op)39)
#define	OP_OR			((ecode_op)40)
#define	OP_OUTPUT		((ecode_op)41)
#define	OP_POP			((ecode_op)42)
#define	OP_PROCCALL		((ecode_op)43)
#define	OP_PROCEDURE		((ecode_op)44)
#define	OP_RANGE		((ecode_op)45)
#define	OP_RELEASE		((ecode_op)46)
#define	OP_SETASSIGN		((ecode_op)47)
#define	OP_SETINCLUSION		((ecode_op)48)
#define	OP_SETINCLUSIONTOO	((ecode_op)49)
#define	OP_MODULE		((ecode_op)50)
#define	OP_STRINGCOMPARE	((ecode_op)51)
#define	OP_TRANS		((ecode_op)52)
#define	OP_UNION		((ecode_op)53)
#define	OP_VALUE		((ecode_op)54)
#define	OP_VARIABLE		((ecode_op)55)
#define	OP_VARPARAM		((ecode_op)56)
#define	OP_ABS			((ecode_op)57)
#define	OP_DISPOSE		((ecode_op)58)
#define	OP_NEW			((ecode_op)59)
#define	OP_ODD			((ecode_op)60)
#define	OP_READINT		((ecode_op)61)
#define	OP_SQR			((ecode_op)62)
#define	OP_SUBTRACT		((ecode_op)63)
#define	OP_WRITEINT		((ecode_op)64)
#define	OP_DEFADDR		((ecode_op)65)
#define	OP_DEFARG		((ecode_op)66)
#define	OP_GLOBALCALL		((ecode_op)67)
#define	OP_GLOBALVALUE		((ecode_op)68)
#define	OP_GLOBALVAR		((ecode_op)69)
#define	OP_LOCALVALUE		((ecode_op)70)
#define	OP_LOCALVAR		((ecode_op)71)
#define	OP_SIMPLEASSIGN		((ecode_op)72)
#define	OP_SIMPLEVALUE		((ecode_op)73)
#define	OP_STRINGEQUAL		((ecode_op)74)
#define	OP_STRINGLESS		((ecode_op)75)
#define	OP_STRINGGREATER	((ecode_op)76)
#define	OP_STRINGNOTEQUAL	((ecode_op)77)
#define	OP_STRINGNOTLESS	((ecode_op)78)
#define	OP_STRINGNOTGREATER	((ecode_op)79)
#define	OP_ENTERBLOCK		((ecode_op)80)
#define	OP_EXITBLOCK		((ecode_op)81)
#define	OP_ENDCLAUSE		((ecode_op)82)
#define	OP_DELAY		((ecode_op)83)
#define	OP_WHEN			((ecode_op)84)
#define	OP_RETURNVAR		((ecode_op)85)
#define	OP_RANDOM		((ecode_op)86)
#define	OP_READCH		((ecode_op)87)
#define	OP_WRITECH		((ecode_op)88)
#define	OP_WRITESTR		((ecode_op)89)
#define	OP_SETEQUAL		((ecode_op)90)
#define	OP_READSTR		((ecode_op)91)
#define	OP_DEFIPS		((ecode_op)92)
#define	OP_SCOPE		((ecode_op)93)
#define	OP_EXPVAR		((ecode_op)94)
#define	OP_WITH			((ecode_op)95)
#define	OP_WITHFIELD		((ecode_op)96)
#define	OP_ENDWITH		((ecode_op)97)
#define	OP_ENDWRITE		((ecode_op)98)
#define	OP_DOMAIN		((ecode_op)99)
#define	OP_EXISTMOD		((ecode_op)100)
#define	OP_ENDDOMAIN		((ecode_op)101)
#define	OP_NUMCHILDREN		((ecode_op)102)
#define	OP_OTHERWISE		((ecode_op)103)
#define	OP_FIRECOUNT		((ecode_op)104)
#define	OP_GLOBALTIME		((ecode_op)105)
#define	OP_QLENGTH		((ecode_op)106)
#define	OP_SETOUTPUT		((ecode_op)107)
#define	OP_TERMINATE		((ecode_op)108)
#define	OP_ERANDOM		((ecode_op)109)
#define	OP_PRANDOM		((ecode_op)110)
#define OP_GRANDOM		((ecode_op)111)

#define NUM_OPS			112

#else

enum operationpart
{
     /* 0  */ OP_ADD, OP_ADDRESS, OP_ADDSETELEMENT, OP_ADDSETRANGE, OP_AND,
     /* 5  */ OP_ASSIGN, OP_ATTACH, OP_CASE, OP_CONNECT, OP_CONSTANT, OP_COPY,
     /* 11 */ OP_DETACH, OP_DIFFERENCE, OP_DISCONNECT, OP_DIV, OP_DO, OP_ENDPROC,
     /* 17 */ OP_ENDTRANS, OP_EQUAL, OP_FIELD, OP_FOR, OP_GOTO, OP_GREATER,
     /* 23 */ OP_IN, OP_INTERSECTION, OP_INDEX, OP_INDEXEDJUMP, OP_INIT, OP_LESS,
     /* 29 */ OP_MINUS, OP_MODULEASSIGN, OP_MODULO, OP_MULTIPLY, OP_NEWLINE,
     /* 34 */ OP_NEXT, OP_NEXTINSTANCE, OP_NOT, OP_NOTEQUAL, OP_NOTGREATER,
     /* 39 */ OP_NOTLESS, OP_OR, OP_OUTPUT, OP_POP, OP_PROCCALL, OP_PROCEDURE,
     /* 45 */ OP_RANGE, OP_RELEASE, OP_SETASSIGN, OP_SETINCLUSION,
     /* 49 */ OP_SETINCLUSIONTOO, OP_MODULE, OP_STRINGCOMPARE, OP_TRANS, OP_UNION,
     /* 54 */ OP_VALUE, OP_VARIABLE, OP_VARPARAM, OP_ABS, OP_DISPOSE, OP_NEW,
     /* 60 */ OP_ODD, OP_READINT, OP_SQR, OP_SUBTRACT, OP_WRITEINT, OP_DEFADDR,
     /* 66 */ OP_DEFARG, OP_GLOBALCALL, OP_GLOBALVALUE, OP_GLOBALVAR, OP_LOCALVALUE,
     /* 71 */ OP_LOCALVAR, OP_SIMPLEASSIGN, OP_SIMPLEVALUE, OP_STRINGEQUAL,
     /* 75 */ OP_STRINGLESS, OP_STRINGGREATER, OP_STRINGNOTEQUAL, OP_STRINGNOTLESS,
     /* 79 */ OP_STRINGNOTGREATER, OP_ENTERBLOCK, OP_EXITBLOCK, OP_ENDCLAUSE,
     /* 83 */ OP_DELAY, OP_WHEN, OP_RETURNVAR, OP_RANDOM, OP_READCH,
     /* 88 */ OP_WRITECH, OP_WRITESTR, OP_SETEQUAL, OP_READSTR, OP_DEFIPS,
     /* 93 */ OP_SCOPE, OP_EXPVAR, OP_WITH, OP_WITHFIELD, OP_ENDWITH,
     /* 98 */ OP_ENDWRITE, OP_DOMAIN, OP_EXISTMOD, OP_ENDDOMAIN, OP_NUMCHILDREN,
     /* 103*/ OP_OTHERWISE, OP_FIRECOUNT, OP_GLOBALTIME, OP_QLENGTH, OP_SETOUTPUT,
     /* 108*/ OP_TERMINATE, OP_ERANDOM, OP_PRANDOM, OP_GRANDOM,

    NUM_OPS
};

typedef enum operationpart ecode_op;

#endif				/* UNIX */

#ifdef NEED_OP_ARGS

short Op_NArgs[(int)NUM_OPS] =
{
     /* 00  Add              */ 0,
     /* 01  Address          */ 0,
     /* 02  AddSetElement    */ 0,
     /* 03  AddSetRange      */ 0,
     /* 04  And              */ 0,
     /* 05  Assign           */ 1,
     /* 06  Attach           */ 0,
     /* 07  Case             */ 0,
     /* 08  Connect          */ 2,
     /* 09  Constant         */ 1,
     /* 10  Copy             */ 0,
     /* 11  Detach           */ 0,
     /* 12  Difference       */ 0,
     /* 13  Disconnect       */ 0,
     /* 14  Div              */ 0,
     /* 15  Do               */ 1,
     /* 16  EndProc          */ 2,
     /* 17  EndTrans         */ 0,
     /* 18  Equal            */ 0,
     /* 19  Field            */ 1,
     /* 20  For              */ 2,
     /* 21  Goto             */ 1,
     /* 22  Greater          */ 0,
     /* 23  In               */ 0,
     /* 24  Intersection     */ 0,
     /* 25  Index            */ 3,
     /* 26  IndexedJump      */ 0,
     /* 27  Init             */ 3,
     /* 28  Less             */ 0,
     /* 29  Minus            */ 0,
     /* 30  ModuleAssign     */ 0,
     /* 31  Modulo           */ 0,
     /* 32  Multiply         */ 0,
     /* 33  NewLine          */ 1,
     /* 34  Next             */ 2,
     /* 35  NextInstance     */ 1,
     /* 36  Not              */ 0,
     /* 37  NotEqual         */ 0,
     /* 38  NotGreater       */ 0,
     /* 39  NotLess          */ 0,
     /* 40  Or               */ 0,
     /* 41  Output           */ 7,
     /* 42  Pop              */ 1,
     /* 43  ProcCall         */ 3,
     /* 44  Procedure        */ 3,
     /* 45  Range            */ 2,
     /* 46  Release          */ 0,
     /* 47  SetAssign        */ 0,
     /* 48  SetInclusion     */ 0,
     /* 49  SetInclusionToo  */ 0,
     /* 50  Module  		*/ 14,
     /* 51  StringCompare    */ 2,
     /* 52  Trans            */ 17,
     /* 53  Union            */ 0,
     /* 54  Value            */ 1,
     /* 55  Variable         */ 2,
     /* 56  VarParam         */ 2,
     /* 57  Abs              */ 0,
     /* 58  Dispose          */ 0,
     /* 59  New              */ 1,
     /* 60  Odd              */ 0,
     /* 61  ReadInt          */ 0,
     /* 62  Sqr              */ 0,
     /* 63  Subtract         */ 0,
     /* 64  WriteInt         */ 0,
     /* 65  DefAddr          */ 1,
     /* 66  DefArg           */ 2,
     /* 67  GlobalCall       */ 2,
     /* 68  GlobalValue      */ 1,
     /* 69  GlobalVar        */ 1,
     /* 70  LocalValue       */ 1,
     /* 71  LocalVar         */ 1,
     /* 72  SimpleAssign     */ 0,
     /* 73  SimpleValue      */ 0,
     /* 74  StringEqual      */ 1,
     /* 75  StringLess       */ 1,
     /* 76  StringGreater    */ 1,
     /* 77  StringNotEqual   */ 1,
     /* 78  StringNotLess    */ 1,
     /* 79  StringNotGreater,*/ 1,
     /* 80  EnterBlock	*/ 1,
     /* 81  ExitBlock	*/ 0,
     /* 82  EndClause	*/ 0,
     /* 83  Delay		*/ 2,
     /* 84  When		*/ 2,
     /* 85  ReturnVar	*/ 1,
     /* 86  Random		*/ 0,
     /* 87  ReadCh           */ 0,
     /* 88  WriteCh          */ 0,
     /* 89  WriteStr		*/ 1,
     /* 90  SetEqual		*/ 0,
     /* 91  ReadStr		*/ 1,
     /* 92  DefIps		*/ 2,
     /* 93  Scope		*/ 2,
     /* 94  ExpVar		*/ 2,
     /* 95  With		*/ 0,
     /* 96  WithField	*/ 2,
     /* 97  EndWith		*/ 1,
     /* 98  EndWrite		*/ 0,
     /* 99  Domain		*/ 1,
     /* 100 ExistMod		*/ 2,
     /* 101 EndDomain	*/ 1,
     /* 102 NumChildren	*/ 0,
     /* 103 Otherwise	*/ 2,
     /* 104 FireCount	*/ 0,
     /* 105 GlobalTime	*/ 0,
     /* 106 QLength		*/ 0,
     /* 107 SetOutput	*/ 1,
     /* 108 Terminate	*/ 0,
     /* 109 Erandom		*/ 0,
     /* 110 Prandom		*/ 0,
     /* 111 Grandom		*/ 0
};

#else

extern short Op_NArgs[];

#endif				/* NEED_OP_ARGS */

#ifdef NEED_OP_NAMES

char *Op_Names[(int)NUM_OPS] =
{
     /* 00 */ "Add",
     /* 01 */ "Address",
     /* 02 */ "AddSetElement",
     /* 03 */ "AddSetRange",
     /* 04 */ "And",
     /* 05 */ "Assign",
     /* 06 */ "Attach",
     /* 07 */ "Case",
     /* 08 */ "Connect",
     /* 09 */ "Constant",
     /* 10 */ "Copy",
     /* 11 */ "Detach",
     /* 12 */ "Difference",
     /* 13 */ "Disconnect",
     /* 14 */ "Div",
     /* 15 */ "Do",
     /* 16 */ "EndProc",
     /* 17 */ "EndTrans",
     /* 18 */ "Equal",
     /* 19 */ "Field",
     /* 20 */ "For",
     /* 21 */ "Goto",
     /* 22 */ "Greater",
     /* 23 */ "In",
     /* 24 */ "Intersection",
     /* 25 */ "Index",
     /* 26 */ "IndexedJump",
     /* 27 */ "Init",
     /* 28 */ "Less",
     /* 29 */ "Minus",
     /* 30 */ "ModuleAssign",
     /* 31 */ "Modulo",
     /* 32 */ "Multiply",
     /* 33 */ "NewLine",
     /* 34 */ "Next",
     /* 35 */ "NextInstance",
     /* 36 */ "Not",
     /* 37 */ "NotEqual",
     /* 38 */ "NotGreater",
     /* 39 */ "NotLess",
     /* 40 */ "Or",
     /* 41 */ "Output",
     /* 42 */ "Pop",
     /* 43 */ "ProcCall",
     /* 44 */ "Procedure",
     /* 45 */ "Range",
     /* 46 */ "Release",
     /* 47 */ "SetAssign",
     /* 48 */ "SetInclusion",
     /* 49 */ "SetInclusionToo",
     /* 50 */ "Module",
     /* 51 */ "StringCompare",
     /* 52 */ "Trans",
     /* 53 */ "Union",
     /* 54 */ "Value",
     /* 55 */ "Variable",
     /* 56 */ "VarParam",
     /* 57 */ "Abs",
     /* 58 */ "Dispose",
     /* 59 */ "New",
     /* 60 */ "Odd",
     /* 61 */ "ReadInt",
     /* 62 */ "Sqr",
     /* 63 */ "Subtract",
     /* 64 */ "WriteInt",
     /* 65 */ "DefAddr",
     /* 66 */ "DefArg",
     /* 67 */ "GlobalCall",
     /* 68 */ "GlobalValue",
     /* 69 */ "GlobalVar",
     /* 70 */ "LocalValue",
     /* 71 */ "LocalVar",
     /* 72 */ "SimpleAssign",
     /* 73 */ "SimpleValue",
     /* 74 */ "StringEqual",
     /* 75 */ "StringLess",
     /* 76 */ "StringGreater",
     /* 77 */ "StringNotEqual",
     /* 78 */ "StringNotLess",
     /* 79 */ "StringNotGreater",
     /* 80 */ "EnterBlock",
     /* 81 */ "ExitBlock",
     /* 82 */ "EndClause",
     /* 83 */ "Delay",
     /* 84 */ "When",
     /* 85 */ "ReturnVar",
     /* 86 */ "Random",
     /* 87 */ "ReadCh",
     /* 88 */ "WriteCh",
     /* 89 */ "WriteStr",
     /* 90 */ "SetEqual",
     /* 91 */ "ReadStr",
     /* 92 */ "DefIps",
     /* 93 */ "Scope",
     /* 94 */ "ExpVar",
     /* 95 */ "With",
     /* 96 */ "WithField",
     /* 97 */ "EndWith",
     /* 98 */ "EndWrite",
     /* 99 */ "Domain",
     /* 100*/ "ExistMod",
     /* 101*/ "EndDomain",
     /* 102*/ "NumChildren",
     /* 103*/ "Otherwise",
     /* 104*/ "FireCount",
     /* 105*/ "GlobalTime",
     /* 106*/ "QLength",
     /* 107*/ "SetOutput",
     /* 108*/ "Terminate",
     /* 109*/ "Erandom",
     /* 110*/ "Prandom",
     /* 111*/ "Grandom",
};

#endif				/* NEED_OP_NAMES */

#endif				/* INCLUDED_ECODE */
