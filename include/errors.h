/* Errors.h */

#ifndef INCLUDED_ERRORS

#define INCLUDED_ERRORS

enum errornumber
{
     /* 0   */ ERR_MEMFAIL, ERR_BADFREE, ERR_MEMTABLEFULL, ERR_NOSAVESCR, ERR_NOSHELL,
     /* 5   */ ERR_NOMEM, ERR_BADSHELL, ERR_BADINT, ERR_RECMACRO, ERR_PLAYMACRO,
     /* 10  */ ERR_MACROKEY, ERR_MACROLEN,
     /* 24  */ ERR_ENDLIBERRS = 24,
     /* 25  */ ERR_BAD_ARGUMENT, ERR_NOOPENFL, ERR_BADLINE, ERR_BADTSTOP, ERR_REPORT,
     /* 30  */ ERR_NOHLPINDX, ERR_NOHELPTEXT, ERR_AMBIGUOUS, ERR_ASSIGNMENT, ERR_COMMENT,
     /* 35  */ ERR_COMPATIBLE, ERR_FUNC, ERR_INTEGER, ERR_LABEL, ERR_STRING,
     /* 40  */ ERR_FUNCTION, ERR_SET, ERR_TIME, ERR_NODEFAULT, ERR_SYSTEMCLASS,
     /* 45  */ ERR_LABELS, ERR_LEVEL, ERR_ORDINAL, ERR_POINTER, ERR_PROC,
     /* 50  */ ERR_RANGE, ERR_REPETITION, ERR_ROLES, ERR_SELECTORTYPE, ERR_SIGNED,
     /* 55  */ ERR_STATES, ERR_TYPE, ERR_UNDEFINED, ERR_LOOPVAR, ERR_CONTROLVAR,
     /* 60  */ ERR_CONTROLPARAM, ERR_UNRESOLVED, ERR_INDICES, ERR_DUPCLAUSE, ERR_SYMBOLSPC,
     /* 65  */ ERR_SPECTOOBIG, ERR_ANY, ERR_ANYCONST, ERR_ANYTYPE, ERR_DISPOSE,
     /* 70  */ ERR_EXTERNAL, ERR_FUNCPARAM, ERR_NEW, ERR_PACK, ERR_PRIMITIVE,
     /* 75  */ ERR_PROCPARAM, ERR_WITH, ERR_ESTELLE, ERR_GOTO, ERR_MODVAR,
     /* 80  */ ERR_NOIDENT, ERR_NOINTEGER, ERR_BADLOOP, ERR_BADFACTOR, ERR_BADCONSTANT,
     /* 85  */ ERR_BADFORMLPARM, ERR_BADCLAUSE, ERR_SYNTAX, ERR_BADROLES, ERR_CODEFULL,
     /* 90  */ ERR_ECUSEAGE, ERR_NOSOURCE, ERR_NOTARGET, ERR_NOSEMICOLON, ERR_NOCOLON,
     /* 95  */ ERR_NORIGHTPAR, ERR_NOLEFTPAR, ERR_NOCOMMA, ERR_NOEND, ERR_NODO,
     /* 100 */ ERR_NOLEFTBRAK, ERR_NOPERIOD, ERR_NORIGHTBRAK, ERR_NOOF, ERR_NOLOGOPEN,
     /* 105 */ ERR_IPEMPTY, ERR_NOCONNECT, ERR_BIGSOURCE, ERR_ILLEGALOP, ERR_STACKOVR,
     /* 110 */ ERR_OUTRANGE, ERR_BADCONNECT, ERR_BADATTACH, ERR_2MANYWHEN, ERR_BADEXPORT,
     /* 115 */ ERR_NOTEXPORTED, ERR_EXIST, ERR_MODEXIST, ERR_MIXEXIST, ERR_DOMAINSIZE,
     /* 120 */ ERR_POINTEROP, ERR_NEWFAIL, ERR_BADDISPOSE, ERR_FAILEXPAND, ERR_BADINITTRANS,
     /* 125 */ ERR_CASEVALUE, ERR_BADBRKPT, ERR_FIREENTRIES, ERR_WRONGROLE, ERR_CLAUSENEST,
     /* 130 */ ERR_ZERODIV, ERR_BADSTEP, ERR_INTRALLOCFAIL, ERR_INITFAIL, ERR_METATRANS,
     /* 135 */ ERR_MODSTACK, ERR_MODVARTBL, ERR_IPTBL, ERR_COMMONQ, ERR_QUEUE,
     /* 140 */ ERR_TRANSTBL, ERR_ECMEMFAIL, ERR_ECCODFAIL, ERR_MAXTRANS, ERR_PRIORITY,
     /* 145 */ ERR_PROPDELAY,
     /* 149 */ ERR_ENDERRS = 149,
     /* 150 */ WARN_UNREACHABLE, WARN_DEADLOCK, WARN_WHENOUT, WARN_OUTWHEN, WARN_UNREFINTR
};

void Error_Install(char *envvar, char *basename);
BOOLEAN ErrorGetMessage(enum errornumber code, STRING buf, short buflen);
void ErrorHelp(enum errornumber code);
void Error(BOOLEAN waitforkey, enum errornumber errno,...);

#endif				/* INCLUDED_ERRORS */
