#include "symbol.h"
#include "compiler.h"

#define estelle()			(block[blocklevel].estelleblock)
#define variable_ident()		gen_ident(VARIABLE)
#define body_ident()			gen_ident(BODY)

static void NEAR all_statement(void);
static void NEAR attach_statement(void);
static void NEAR connect_statement(void);
static void NEAR disconnect_statement(void);
static void NEAR detach_statement(void);
static void NEAR forone_statement(void);
static void NEAR init_statement(void);
static void NEAR output_statement(void);
static void NEAR release_statement(void);
static void NEAR terminate_statement(void);

static pointer NEAR standard_function(void);
static void NEAR standard_procedure(void);

static pointer NEAR set_constructor(void);

/**************************************************************************/
/*									  */
/*		ESTELLE STATEMENTS					  */
/*		==================					  */
/*									  */
/* AllStatement ::- "all" Identifier ":" HeaderIdentifier "do" Statement. */
/* AttachStatement ::- "attach" ExternalIp "to" ChildExternalIp.	  */
/* ConnectStatement ::- "connect" ConnectIp "to" ConnectIp.		  */
/* DisconnectStatement ::- "disconnect" ( ConnectIp | ModuleVariable ).	  */
/* DetachStatement ::- "detach" ( ExternalIp | ChildExternalIp ).	  */
/* DomainTypeAll ::- OrdinalType.					  */
/* DomainList ::- IdentifierList ":" DomainTypeAll			  */
/*		 [ ";" IdentifierList ":" DomainTypeAll ]^0.		  */
/* ForoneStatement ::- "forone" ( DomainList | Identifier		  */
/*				":" HeaderIdentifier )			  */
/*		      "suchthat" BooleanExpression "do" Statement	  */
/*		      [ "otherwise" Statement ].			  */
/* ActualModuleParameter ::- Expression.				  */
/* ActualModuleParameterList ::- ActualParameterList.			  */
/* InitStatement ::- "init" ModuleVariable "with" BodyIdentifier	  */
/*		    [ "(" ActualModuleParameterList ")" ].		  */
/* OutputStatement ::- "output" InteractionReference			  */
/*		      [ "(" ActualParameterList ")" ].			  */
/* ReleaseStatement ::- "release" ModuleVariable.			  */
/**************************************************************************/

/****************************************************************/
/* Domain ::- Domain-list | Module-domain.			*/
/* Domain-list ::- IDENT-LIST ":" ORDINAL-TYPE			*/
/*			{ ";" IDENT-LIST ":" ORDINAL-TYPE }.	*/
/* Module-domain ::- IDENT ":" Header-Ident.			*/
/****************************************************************/

static short NEAR 
domain(short startlabel[], short endlabel[], pointer varptr[], short Min[], short Max[])
{
    short ident[MAXDOMAIN], size = 0, index;
    pointer type, var;
    BOOLEAN not_mod_type = FALSE;

    do
    {
	index = 0;
	do
	{
	    ident[index++] = expect_ident();
	    if (index > MAXDOMAIN)
		compiler_error(210, ERR_DOMAINSIZE);
	    if (symbol == SYM_COMMA)
		nextsymbol();
	    else
		break;
	} while (TRUE);
	expect(SYM_COLON);
	if (symbol == SYM_IDENTIFIER)
	{
	    type = find(argument);
	    nextsymbol();
	    if (type->kind != MODULETYPE)
	    {
		if (!isin(type, ORDINALS))
		    compiler_error(210, ERR_EXIST);
	    } else
	    {
		/* Code for module exist here */
		if (!estelle())
		    compiler_error(328, ERR_ESTELLE);
		if (index > 1)
		    compiler_error(210, ERR_MODEXIST);
		if (not_mod_type)
		    compiler_error(210, ERR_MIXEXIST);
		var = define(ident[0], MODVAR);
		var->_Var = (varpointer) EC_malloc(sizeof(struct varrecord));
		var->type = type;
		size = 1;
		var->place = -1;
		var->_Var_Level = blocklevel;
		startlabel[0] = new_label();
		endlabel[0] = new_label();
		varptr[0] = var;
		break;
	    }
	} else
	    type = ordinal_type();
	while (index--)
	{
	    short _min = 0, _max = 0;	/* Beware of macro name conflict! */
	    var = define(ident[index], VARIABLE);
	    var->type = type;
	    var->_Var = (varpointer) EC_malloc(sizeof(struct varrecord));
	    startlabel[size] = new_label();
	    endlabel[size] = new_label();
	    varptr[size] = var;
	    if (type->kind == SUBRANGETYPE)
	    {
		_min = type->_Sub_LowerVal;
		_max = type->_Sub_UpperVal;
	    } else if (type->kind == ENUMERATEDTYPE)
	    {
		_min = 0;
		_max = type->_Enum_Last->_Const_Val->_Val_Int;
	    } else if (type == typeinteger)
	    {
		_min = -32768;
		_max = 32767;
	    } else if (type == typechar)
	    {
		_min = 0;
		_max = 255;
	    } else if (type == typeboolean)
	    {
		_min = (int)FALSE;
		_max = (int)TRUE;
	    }
	    Min[size] = _min;
	    Max[size] = _max;
	    ++size;
	    var->place = -size;
	    var->_Var_Level = blocklevel;
	}
	not_mod_type = TRUE;
	if (symbol == SYM_SEMICOLON)
	    nextsymbol();
	else
	    break;
    } while (TRUE);
    if (size > MAXDOMAIN)
	compiler_error(211, ERR_DOMAINSIZE);
    return size;
}

/****************************************************************/
/* ExistOne ::- "exist" Domain "suchthat" BooleanExpression.	*/
/*								*/
/* This is actually an expression, but is put here as it is	*/
/* very similar to FORONE and ALL.				*/
/****************************************************************/

static void NEAR 
    domain_construct(enum symboltype which)
    {
	short size, endlooplabel[MAXDOMAIN], startlooplabel[MAXDOMAIN], _min[MAXDOMAIN],
	    _max[MAXDOMAIN], i;
	short faillabel = new_label(), endlabel = new_label();
	pointer vars[MAXDOMAIN];

	    new_block(block[blocklevel].estelleblock);
	    expect(which);	/* SYM_ALL, SYM_FORONE or SYM_EXIST */
	    size = domain(startlooplabel, endlooplabel, vars, _min, _max);
	if  (which != SYM_ALL)
	        expect(SYM_SUCHTHAT);
	    emit(OP_DOMAIN, size);
	    push(3 + size);
	for (i = 0; i < size; i++)
	{
	    pointer type = vars[i]->type;
	        emit(OP_VARIABLE, 0, vars[i]->place);
	    if  (type->kind == MODULETYPE)
	    {
		emit(OP_CONSTANT, 0);
		emit(OP_NUMCHILDREN);
	    } else
	    {
		emit(OP_CONSTANT, _min[i]);
		emit(OP_CONSTANT, _max[i]);
	    }
	    emit(OP_FOR, endlooplabel[i], 1);
	    emit(OP_DEFADDR, startlooplabel[i]);
	}
	if (vars[0]->type->kind == MODULETYPE)
	    emit(OP_EXISTMOD, vars[0]->type->ident, faillabel);
	if (which != SYM_ALL)
	    checktypes(expression(), typeboolean);
	if (which != SYM_EXIST)
	    expect(SYM_DO);
	if (which != SYM_ALL)
	    emit(OP_DO, faillabel);
	if (which == SYM_EXIST)
	{
	    emit(OP_CONSTANT, 1);
	    emit(OP_GOTO, endlabel);
	} else
	    statement();
	if (which != SYM_ALL)
	    emit(OP_GOTO, endlabel);	/* done... */
	emit(OP_DEFADDR, faillabel);
	for (i = size - 1; i >= 0; i--)
	{
	    emit(OP_NEXT, startlooplabel[i], 1);
	    emit(OP_DEFADDR, endlooplabel[i]);
	}
	if (which == SYM_EXIST)
	    emit(OP_CONSTANT, 0);	/* EXIST fails */
	emit(OP_DEFADDR, endlabel);
	emit(OP_ENDDOMAIN, size);
	pop(2 + size);
	if (which != SYM_EXIST)
	{
	    emit(OP_POP, 1);	/* Pop value PUSHed for EXIST */
	    pop(1);
	}
	exitblock();
    }

static void NEAR existone(void)
{
    domain_construct(SYM_EXIST);
}

static void NEAR all_statement(void)
{
    domain_construct(SYM_ALL);
}

static void NEAR forone_statement(void)
{
    domain_construct(SYM_FORONE);
}

static void NEAR 
attach_statement(void)
{
    pointer ipx1, ipx2;
    expect(SYM_ATTACH);
    ipx1 = IP_reference();
    expect(SYM_TO);
    ipx2 = childexternalip();
    if ((ipx1->_IPTyp_Chan != ipx2->_IPTyp_Chan) ||
	(ipx1->_IPTyp_Role != ipx2->_IPTyp_Role))
	compiler_error(300, ERR_BADROLES);
    emit(OP_ATTACH);
}

static void NEAR 
connect_statement(void)
{
    BOOLEAN internal1, internal2;
    pointer ipx1, ipx2;
    expect(SYM_CONNECT);
    ipx1 = connectip(&internal1);
    expect(SYM_TO);
    ipx2 = connectip(&internal2);
    if ((ipx1->_IPTyp_Chan != ipx2->_IPTyp_Chan) ||
	(ipx1->_IPTyp_Role == ipx2->_IPTyp_Role))
	compiler_error(300, ERR_BADROLES);
    emit(OP_CONNECT, (ushort) (internal1), (ushort) (internal2));
    pop(6 - (int)internal1 - (int)internal2);
}

static void NEAR 
    split_IPs(enum symboltype which, ecode_op op)
    {
	short mode = 0;		/* 0 - internal, 1 - child external, 2 -
				 * child */
	    expect(which);
	if  (symbol == SYM_IDENTIFIER)
	{
	    if (find(argument)->kind == MODVAR)
	    {
		(void)module_variable(FALSE);
		if (symbol == SYM_PERIOD)
		{
		    nextsymbol();
		    (void)IP_reference();
		    mode = 1;
		} else
		        mode = 2;
	    } else
	    {
		(void)IP_reference();
		mode = 0;
	    }
	} else
	    compiler_error(301, ERR_NOIDENT);
	emit(op, mode);
	pop(mode == 2 ? 1 : (mode + 2));	/* mode0=>pop(2),
						 * mode1=>pop(3),
						 * mode2=>pop(1) */
    }

static void NEAR 
disconnect_statement(void)
{
    split_IPs(SYM_DISCONNECT, OP_DISCONNECT);
}

static void NEAR 
detach_statement(void)
{
    split_IPs(SYM_DETACH, OP_DETACH);
}

static void NEAR 
init_statement(void)
{
    pointer body;
    short length = 0;
    expect(SYM_INIT);
    (void)module_variable(TRUE);
    expect(SYM_WITH);
    body = body_ident();
    if (body->_Body_Header->_Mod_LastPar != NULL)
    {
	expect(SYM_LEFTPARENTHESIS);
	actual_param_list(body->_Body_Header->_Mod_LastPar, &length);
	expect(SYM_RIGHTPARENTHESIS);
    }
    emit(OP_INIT, body->place, length, body->_Body_Header->_Mod_ExVars);
}

static void NEAR 
output_statement(void)
{
    pointer interactionx;
    short length = 0;
    expect(SYM_OUTPUT);
    interactionx = interaction_reference();
    interactionx->_Intr_HasOutput = TRUE;
    if (interactionx->_Intr_LastArg != NULL)
    {
	expect(SYM_LEFTPARENTHESIS);
	actual_param_list(interactionx->_Intr_LastArg, &length);
	expect(SYM_RIGHTPARENTHESIS);
    }
    switch (interactionx->_Intr_IPType->_IPTyp_Chan->_Chan_QType)
    {
    case FIFO_Q:		/* If FIFO, priority must be zero */
	if (interactionx->_Intr_Priority)
	    compiler_error(1136, ERR_PRIORITY);
	break;
    case RANDOM_Q:		/* If random, priority and delay must be zero */
	if (interactionx->_Intr_Priority)
	    compiler_error(1136, ERR_PRIORITY);
    case PRIORITY_Q:		/* Propogation delay must be zero */
	if (interactionx->_Intr_IPType->_IPTyp_Delay)
	    compiler_error(1137, ERR_PROPDELAY);
	break;
    }
    emit(OP_OUTPUT,
	 interactionx->ident,
	 interactionx->_Intr_ArgLen,
	 length,
	 default_reliability,
	 interactionx->_Intr_Priority,
	 interactionx->_Intr_IPType->_IPTyp_Delay,
	 interactionx->_Intr_IPType->_IPTyp_Chan->_Chan_QType
	);
    pop(length + 2);
    default_reliability = 100;	/* restore to normal value */
}

static void NEAR 
release_statement(void)
{
    expect(SYM_RELEASE);
    (void)module_variable(FALSE);
    emit(OP_RELEASE);
}

static void NEAR 
terminate_statement(void)
{
    expect(SYM_TERMINATE);
    (void)module_variable(FALSE);
    emit(OP_TERMINATE);
}

/*************************************************************

	PASCAL STATEMENTS

**************************************************************/

/**********************************************************/
/* CompoundStatement ::- "begin" StatementSequence "end". */
/**********************************************************/

void 
compound_statement(void)
{
    expect(SYM_BEGIN);
    statement();
    while (symbol == SYM_SEMICOLON)
    {
	nextsymbol();
	statement();
    }
    expect(SYM_END);
}

/************************************************************************/
/* FunctionStatement ::- StandardFunction				*/
/*	      | FunctionIdentifier [ "(" ActualParameterList ")" ].	*/
/* ProcedureStatement ::- StandardProcedure				*/
/*	       | ProcedureIdentifier [ "(" ActualParameterList ")" ].	*/
/************************************************************************/

static pointer NEAR 
proc_func_statement(BOOLEAN is_func, pointer proc)
{
    pointer type = 0;
    short paramlength = 0;
    if (is_func && (proc->kind == STANDARDFUNC))
	type = standard_function();
    else if (!is_func && (proc->kind == STANDARDPROC))
	standard_procedure();
    else
    {
	short rtn_len = 0;
	if (is_func)
	    rtn_len = typelength(type = proc->type);
	expect(SYM_IDENTIFIER);
	if (proc->_Proc_LastPar != NULL)
	{
	    expect(SYM_LEFTPARENTHESIS);
	    actual_param_list(proc->_Proc_LastPar, &paramlength);
	    expect(SYM_RIGHTPARENTHESIS);
	}
	emit(OP_PROCCALL, blocklevel - proc->_Proc_Level, proc->_Proc_Label, rtn_len);
	push(3 + rtn_len);
    }
    return type;
}

static void NEAR 
procedure_statement(pointer object)
{
    (void)proc_func_statement(FALSE, object);
}

static pointer NEAR 
function_statement(pointer object)
{
    return proc_func_statement(TRUE, object);
}

/************************************************************************/
/* FunctionIdentifier ::- Identifier. }					*/
/* AssignmentStatement ::- ( VariableAccess | FunctionIdentifier ) }	*/
/*			  ":=" Expression }				*/
/*			| ModuleVariable ":=" ModuleVariable. }		*/
/* GotoStatement ::- "goto" Label. 					*/
/* BooleanExpression ::- Expression. 					*/
/* ElsePart ::- "else" Statement. 					*/
/* IfStmnt ::- "if" BooleanExpression "then" Statement [ ElsePart ].	*/
/* CaseIndex ::- Expression. 						*/
/* CaseListElement ::- CaseConstantList ":" Statement. 			*/
/* CaseStatement ::- "case" CaseIndex "of" CaseListElement 		*/
/*		    [ ";" CaseListElement ]^0 [ ";" ] "end". 		*/
/* WhileStatement ::- "while" BooleanExpression "do" Statement. 	*/
/* FinalValue ::- Expression.						*/
/* InitialValue ::- Expression.						*/
/* ControlVariable ::- EntireVariable.					*/
/* ForStatement ::- "for" ControlVariable ":=" InitialValue		*/
/*		   ( "to" | "downto" ) FinalValue "do" Statement.	*/
/* StatementSequence ::- Statement [ ";" Statement ]^0.			*/
/* RepeatStatement ::- "repeat" StatementSequence			*/
/*			"until" BooleanExpression. 			*/
/* RecordVariableList ::- RecordVariable [ "," RecordVariable ]^0.	*/
/* WithStatement ::- "with" RecordVariableList "do" Statement.		*/
/* SimpleStatement ::- EmptyStatement					*/
/*		    | AssignmentStatement				*/
/*		    | ProcedureStatement				*/
/*		    | GotoStatement					*/
/*		    | AttachStatement					*/
/*		    | ConnectStatement					*/
/*		    | DetachStatement					*/
/*		    | DisconnectStatement				*/
/*		    | InitStatement					*/
/*		    | OutputStatement					*/
/*		    | ReleaseStatement.					*/
/* ConditionalStatement ::- IfStatement | CaseStatement.		*/
/* RepetitiveStatement ::- RepeatStatement				*/
/*			| WhileStatement				*/
/*			| ForStatement					*/
/*			| AllStatement.					*/
/* StructuredStatement ::- CompoundStatement.				*/
/*			| ConditionalStatement.				*/
/*			| RepetitiveStatement.				*/
/*			| WithStatement.				*/
/*			| ForoneStatement.				*/
/* Statement ::- Label ":" EmptyStatement				*/
/*	      |  SimpleStatement | StructuredStatement.			*/
/************************************************************************/

static void NEAR 
assignment_statement(pointer object)
{
    pointer vartype = 0, exprtype;
    short length, level;
    enum compatibility_class assignmentcase;

    if (object->kind == PROCEDUR)
    {
	if (object->_Proc_IsFunc)
	{
	    level = blocklevel - object->_Proc_Level;
	    if (object->_Proc_CanAsgn && level == 1)
	    {
		vartype = object->type;
		emit(OP_RETURNVAR, object->place);
	    } else
		compiler_error(303, ERR_FUNCTION);
	} else
	    compiler_error(304, ERR_TYPE);
	expect(SYM_IDENTIFIER);
    } else
	vartype = variable_access(TRUE, FALSE);
    expect(SYM_BECOMES);
    length = typelength(exprtype = expression());
    assignmentcase = compatibletypes(vartype, exprtype);
    if (assignmentcase == RANGECOMPATIBLE)
    {
	emit(OP_RANGE, vartype->_Sub_LowerVal, vartype->_Sub_UpperVal);
	emit(OP_ASSIGN, length);
	pop(length + 1);
    } else if (GENCOMPATIBLE(assignmentcase))
    {
	if (exprtype->kind == MODULETYPE)
	    emit(OP_MODULEASSIGN);
	else if (isin(vartype, SETS))
	    emit(OP_SETASSIGN);
	else
	{
	    if (isin(vartype, RANGES))
		emit(OP_RANGE, vartype->_Sub_LowerVal, vartype->_Sub_UpperVal);
	    emit(OP_ASSIGN, length);
	    pop(length + 1);
	}
    } else
	compiler_error(305, ERR_ASSIGNMENT);
}

static void NEAR 
goto_statement(void)
{
    expect(SYM_GOTO);
    if (symbol == SYM_INTEGER)
    {
	if (argument != block[blocklevel].label.value)
	    compiler_error(307, ERR_LABEL);
	else
	    emit(OP_GOTO, block[blocklevel].label.number);
	nextsymbol();
    } else
	compiler_error(308, ERR_NOINTEGER);
}

static void NEAR 
if_statement(void)
{
    pointer exprtype;
    short label1, label2;

    expect(SYM_IF);
    exprtype = expression();
    checktypes(exprtype, typeboolean);
    expect(SYM_THEN);
    label1 = new_label();
    emit(OP_DO, label1);
    statement();
    if (symbol == SYM_ELSE)
    {
	nextsymbol();
	label2 = new_label();
	emit(OP_GOTO, label2);
	emit(OP_DEFADDR, label1);
	statement();
	emit(OP_DEFADDR, label2);
    } else
	emit(OP_DEFADDR, label1);
}

static void NEAR 
case_statement(void)

/* Restriction: case values must be in the range 0..MAXCASEINDICES-1 */

{
    short i, startlabel = new_label(), endlabel = new_label();
    short statementlabel, caselabel[MAXCASEINDICES];
    short errorlabel = new_label(), minindex, maxindex;
    pointer caseindex;

    nextsymbol();		/* skip CASE */
    caseindex = expression();
    emit(OP_GOTO, startlabel);
    expect(SYM_OF);
    for (i = 0; i < MAXCASEINDICES; i++)
	caselabel[i] = 0;
    while (symbol != SYM_END)
    {
	statementlabel = new_label();
	case_constant_list(caseindex, caselabel, statementlabel);
	expect(SYM_COLON);
	emit(OP_DEFADDR, statementlabel);
	statement();
	emit(OP_GOTO, endlabel);
	if (symbol == SYM_SEMICOLON)
	    nextsymbol();
    }
    nextsymbol();		/* Skip END */
    emit(OP_DEFADDR, errorlabel);
    emit(OP_CASE);		/* Run-time error */
    minindex = 0;
    while (minindex < MAXCASEINDICES && caselabel[minindex] == 0)
	minindex++;
    maxindex = MAXCASEINDICES;
    do
	maxindex--;
    while (maxindex > 0 && caselabel[maxindex] == 0);
    emit(OP_DEFADDR, startlabel);
    if (minindex < maxindex || caselabel[minindex])
    {
	emit(OP_RANGE, minindex, maxindex);
	if (minindex)
	{
	    emit(OP_CONSTANT, minindex);
	    emit(OP_SUBTRACT);
	}
	emit(OP_INDEXEDJUMP);
	while (minindex <= maxindex)
	{
	    emit(OP_GOTO, caselabel[minindex] ? caselabel[minindex] : errorlabel);
	    minindex++;
	}
    }
    emit(OP_DEFADDR, endlabel);
}


static void NEAR 
while_statement(void)
{
    short label1, label2;
    pointer exprtype;
    label1 = new_label();
    emit(OP_DEFADDR, label1);
    expect(SYM_WHILE);
    exprtype = expression();
    checktypes(exprtype, typeboolean);
    expect(SYM_DO);
    label2 = new_label();
    emit(OP_DO, label2);
    statement();
    emit(OP_GOTO, label1);
    emit(OP_DEFADDR, label2);
}

static void NEAR 
for_statement(void)
{
    enum compatibility_class compatiblecase;
    short mode = 0, label1, label2, level, displ;
    pointer var, initialvalue, finalvalue;
    expect(SYM_FOR);
    var = variable_ident();
    if (var->_Var_IsLoop)
	compiler_error(310, ERR_LOOPVAR);
    if (!isin(var->type, ORDINALS))
	compiler_error(311, ERR_ORDINAL);
    level = blocklevel - var->_Var_Level;
    displ = var->place;
    if (level == 0)
	if (var->kind == VARIABLE)
	    emit(OP_VARIABLE, level, displ);
	else
	    compiler_error(312, ERR_CONTROLPARAM);
    else
	compiler_error(313, ERR_CONTROLVAR);
    var->_Var_IsLoop = TRUE;
    expect(SYM_BECOMES);
    initialvalue = expression();
    compatiblecase = compatibletypes(var->type, initialvalue);
    if (compatiblecase == COMPATIBLE || compatiblecase == SUBRANGECOMPATIBLE || compatiblecase == RANGECOMPATIBLE)
    {
	if (isin(var->type, RANGES))
	    emit(OP_RANGE, var->type->_Sub_LowerVal, var->type->_Sub_UpperVal);
    } else
	compiler_error(315, ERR_TYPE);
    if (symbol == SYM_TO || symbol == SYM_DOWNTO)
    {
	mode = (symbol == SYM_TO) ? 1 : -1;
	nextsymbol();
    } else
	compiler_error(316, ERR_BADLOOP);
    finalvalue = expression();
    compatiblecase = compatibletypes(var->type, finalvalue);
    if (compatiblecase == COMPATIBLE || compatiblecase == SUBRANGECOMPATIBLE || compatiblecase == RANGECOMPATIBLE)
    {
	if (isin(var->type, RANGES))
	    emit(OP_RANGE, var->type->_Sub_LowerVal, var->type->_Sub_UpperVal);
    } else
	compiler_error(317, ERR_TYPE);
    label1 = new_label();
    label2 = new_label();
    emit(OP_FOR, label2, mode);
    emit(OP_DEFADDR, label1);
    expect(SYM_DO);
    statement();
    emit(OP_NEXT, label1, mode);
    emit(OP_DEFADDR, label2);
    var->_Var_IsLoop = FALSE;
}

static void NEAR 
repeat_statement(void)
{
    pointer exprtype;
    short label1;
    expect(SYM_REPEAT);
    label1 = new_label();
    emit(OP_DEFADDR, label1);
    statement();
    while (symbol == SYM_SEMICOLON)
    {
	nextsymbol();
	statement();
    }
    expect(SYM_UNTIL);
    exprtype = expression();
    checktypes(exprtype, typeboolean);
    emit(OP_DO, label1);
}

static void NEAR 
with_statement(void)
{
    pointer record;
    short old_blocklevel = blocklevel, with_cnt = 0, tmp_level;
    expect(SYM_WITH);
    do
    {
	record = variable_access(FALSE, FALSE);
	with_cnt++;
	if (record->kind != RECORDTYPE)
	    compiler_error(317, ERR_WITH);
	/*
	 * We create a new block for each record so that we don't have to
	 * duplicate symbol table entries but can use the already existing
	 * field entries
	 */
	new_block(block[blocklevel].estelleblock);
	block[blocklevel].lastobject = record->_Rec_LastFld;
	emit(OP_WITH);
	if (symbol == SYM_COMMA)
	    nextsymbol();
	else
	    break;
    } while (symbol != SYM_DO);
    nextsymbol();		/* skip DO */
    for (tmp_level = old_blocklevel + 1; tmp_level <= blocklevel; tmp_level++)
    {
	pointer tmp = block[tmp_level].lastobject;
	while (tmp)
	{
	    if (tmp->kind == FIELD)
		tmp->_Fld_WithLevel = blocklevel - tmp_level;
	    tmp = tmp->previous;
	}
    }
    statement();
    while (blocklevel != old_blocklevel)
	exitblock();
    emit(OP_ENDWITH, with_cnt);
}

static void NEAR 
simple_statement(void)
{
    pointer object;
    switch (symbol)
    {
    case SYM_ATTACH:
	if (!estelle())
	    compiler_error(318, ERR_ESTELLE);
	else
	    attach_statement();
	break;
    case SYM_DETACH:
	if (!estelle())
	    compiler_error(319, ERR_ESTELLE);
	else
	    detach_statement();
	break;
    case SYM_CONNECT:
	if (!estelle())
	    compiler_error(320, ERR_ESTELLE);
	else
	    connect_statement();
	break;
    case SYM_DISCONNECT:
	if (!estelle())
	    compiler_error(321, ERR_ESTELLE);
	else
	    disconnect_statement();
	break;
    case SYM_INIT:
	if (!estelle())
	    compiler_error(322, ERR_ESTELLE);
	else
	    init_statement();
	break;
    case SYM_OUTPUT:
	if (!estelle())
	    compiler_error(323, ERR_ESTELLE);
	else
	    output_statement();
	break;
    case SYM_RELEASE:
	if (!estelle())
	    compiler_error(324, ERR_ESTELLE);
	else
	    release_statement();
	break;
    case SYM_TERMINATE:
	if (!estelle())
	    compiler_error(324, ERR_ESTELLE);
	else
	    terminate_statement();
	break;
    case SYM_GOTO:
	if (!estelle())
	    goto_statement();
	else
	    compiler_error(325, ERR_GOTO);
	break;
    case SYM_IDENTIFIER:
	object = find(argument);
	switch (object->kind)
	{
	case MODVAR:
	    if (!estelle())
		compiler_error(326, ERR_MODVAR);
	    else
		assignment_statement(object);
	    break;
	case FIELD:
	case VARIABLE:
	case VALUEPARAMETER:
	case VARPARAMETER:
	case INTERACTIONARG:
	    assignment_statement(object);
	    break;
	case PROCEDUR:
	case STANDARDPROC:
	    if (object->_Proc_CanAsgn)
		assignment_statement(object);
	    else
		procedure_statement(object);
	    break;
	default:
	    compiler_error(327, ERR_TYPE);
	}
    default:
	break;
    }
}

static void NEAR 
structured_statement(void)
{
    switch (symbol)
    {
	case SYM_ALL:all_statement();
	break;
    case SYM_FORONE:
	forone_statement();
	break;
    case SYM_BEGIN:
	compound_statement();
	break;
    case SYM_WITH:
	with_statement();
	break;
    case SYM_IF:
	if_statement();
	break;
    case SYM_CASE:
	case_statement();
	break;
    case SYM_REPEAT:
	repeat_statement();
	break;
    case SYM_FOR:
	for_statement();
	break;
    case SYM_WHILE:
	while_statement();
	break;
    default:
	break;
    }
}

void 
statement(void)
{
    short label1;
    switch (symbol)
    {
    case SYM_INTEGER:
	if ((argument == block[blocklevel].label.value))
	{
	    label1 = block[blocklevel].label.number;
	    block[blocklevel].label.resolved = TRUE;
	    emit(OP_DEFADDR, label1);
	} else
	    compiler_error(329, ERR_LABEL);
	nextsymbol();
	expect(SYM_COLON);
	break;

    case SYM_IDENTIFIER:
    case SYM_GOTO:
    case SYM_ATTACH:
    case SYM_CONNECT:
    case SYM_DETACH:
    case SYM_DISCONNECT:
    case SYM_INIT:
    case SYM_OUTPUT:
    case SYM_RELEASE:
	simple_statement();
	break;

    case SYM_BEGIN:
    case SYM_CASE:
    case SYM_IF:
    case SYM_WHILE:
    case SYM_REPEAT:
    case SYM_WITH:
    case SYM_FOR:
    case SYM_ALL:
    case SYM_FORONE:
	structured_statement();
	break;

    default:
	break;
    }
}

/********************************************************/
/* StandardFunction ::- "abs" "(" Expression ")"	*/
/*		     | "sqr" "(" Expression ")"		*/
/*		     | "odd" "(" Expression ")"		*/
/*		     | "ord" "(" Expression ")"		*/
/*		     | "chr" "(" Expression ")"		*/
/*		     | "succ" "(" Expression ")"	*/
/*		     | "pred" "(" Expression ")".	*/
/********************************************************/

static pointer NEAR 
standard_function(void)
{
    short ident = argument;
    pointer type;
    expect(SYM_IDENTIFIER);
    if (ident == _FIRECOUNT || ident == _GLOBALTIME)
    {
	type = typeinteger;
	emit(ident == _FIRECOUNT ? OP_FIRECOUNT : OP_GLOBALTIME);
    } else
    {
	expect(SYM_LEFTPARENTHESIS);
	if (ident == _QLENGTH)
	{
	    (void)IP_reference();
	    emit(OP_QLENGTH);
	    type = typeinteger;
	} else
	{
	    type = expression();
	    switch (ident)
	    {
	    case _ABS:
	    case _SQR:
	    case _RANDOM:
	    case _ODD:
		checktypes(type, typeinteger);
		emit(ident == _ABS ? OP_ABS :
		     (ident == _SQR ? OP_SQR :
		      (ident == _ODD ? OP_ODD : OP_RANDOM)));
		break;
	    case _PRANDOM:
	    case _GRANDOM:
	    case _ERANDOM:
		checktypes(type, typeinteger);
		expect(SYM_COMMA);
		type = expression();
		checktypes(type, typeinteger);
		expect(SYM_COMMA);
		type = expression();
		checktypes(type, typeinteger);
		emit(ident == _PRANDOM ? OP_PRANDOM :
		     ident == _ERANDOM ? OP_ERANDOM :
		     OP_GRANDOM);
		break;
	    case _ORD:
		if (!isin(type, ORDINALS))
		    compiler_error(330, ERR_TYPE);
		type = typeinteger;
		break;
	    case _CHR:
		checktypes(type, typeinteger);
		type = typechar;
		break;
	    case _SUCC:
	    case _PRED:
		emit(OP_CONSTANT, 1);
		emit(ident == _SUCC ? OP_ADD : OP_SUBTRACT);
		if (!isin(type, ORDINALS))
		    compiler_error(331, ERR_TYPE);
		if (isin(type, RANGES))
		    emit(OP_RANGE, type->_Sub_LowerVal, type->_Sub_UpperVal);
		if (type == typechar)
		    emit(OP_RANGE, 0, 127);
		break;
	    }
	}
	expect(SYM_RIGHTPARENTHESIS);
    }
    return type;
}

/****************************************************************/
/* StandardProcedure ::- "unpack" "(" VariableAccess ","	*/
/*					VariableAccess "," 	*/
/*				     	VariableAccess ")" 	*/
/*		      | "pack" "(" VariableAccess ","		*/
/*					VariableAccess ","	*/
/*				   	VariableAccess ")"	*/
/*		      | "new" "(" VariableAccess [ ","		*/
/*					Constant ]^0 ")" 	*/
/*		      | "dispose" "(" VariableAccess [ ","	*/
/*					Constant ]^0 ")" 	*/
/*		      | "read" "(" VariableAccessList ")" 	*/
/*		      | "write" "(" ExpressionList ")".		*/
/*		      | "writeln" [ "(" ExpressionList ")" ].   */
/****************************************************************/

static void NEAR 
standard_procedure(void)
{
    short ident;
    BOOLEAN must_write_newline, must_get_rightpar = TRUE;
    pointer type;
    ident = argument;
    expect(SYM_IDENTIFIER);
    if (ident == _WRITELN && symbol != SYM_LEFTPARENTHESIS)
	must_get_rightpar = FALSE;
    else
	expect(SYM_LEFTPARENTHESIS);
    switch (ident)
    {
    case _UNPACK:
    case _PACK:
	compiler_error(300, ERR_PACK);
	break;
    case _NEW:
	type = variable_access(TRUE, FALSE);
	if (!isin(type, POINTERS))
	    compiler_error(333, ERR_POINTER);
	emit(OP_NEW, typelength(type->type));
	emit(OP_ASSIGN, 1);
	pop(2);
	break;
    case _DISPOSE:
	type = variable_access(TRUE, FALSE);
	if (isin(type, POINTERS))
	    emit(OP_DISPOSE);
	else
	    compiler_error(334, ERR_POINTER);
	break;
    case _READ:
	while (symbol != SYM_RIGHTPARENTHESIS)
	{
	    type = variable_access(TRUE, FALSE);
	    if (type == typeinteger)
		emit(OP_READINT);
	    else if (type == typechar)
		emit(OP_READCH);
	    else if (isin(type, STRINGS))
		emit(OP_READSTR, typelength(type));
	    else
		compiler_error(334, ERR_TYPE);
	    pop(1);		/* Would pop 2 if exported */
	    if (symbol == SYM_COMMA)
		nextsymbol();
	    else
		break;
	}
	break;
    case _WRITE:
    case _WRITELN:
	must_write_newline = (BOOLEAN) (ident == _WRITELN);
	if (must_get_rightpar)
	    while (symbol != SYM_RIGHTPARENTHESIS)
	    {
		type = expression();
		if (type == typechar)
		    emit(OP_WRITECH);
		else if (isin(type, ENUMERABLES))
		    emit(OP_WRITEINT);
		else if (isin(type, STRINGS))
		{
		    short length = typelength(type);
		    emit(OP_WRITESTR, length);
		    pop(length);
		}
		if (symbol == SYM_COMMA)
		    nextsymbol();
		else
		    break;
	    }
	if (must_write_newline)
	{
	    emit(OP_CONSTANT, 10);
	    emit(OP_WRITECH);
	}
	/* Tell interpreter to show output window */
	if (!fast_write)
	    emit(OP_ENDWRITE);
	break;
    }
    if (must_get_rightpar)
	expect(SYM_RIGHTPARENTHESIS);
}

/****************************************************************/
/* MemberDesignator ::- Expression [ ".." Expression ]. }	*/
/* SetConstructor ::- "[" [ MemberDesignator [			*/
/*			"," MemberDesignator ]^0 ] "]". }	*/
/****************************************************************/

static pointer NEAR 
set_constructor(void)
{
    pointer exprtype, exprtype2;
    short counter = 0;
    expect(SYM_LEFTBRACKET);
    while (counter++ < SETSIZE)
	emit(OP_CONSTANT, 0);	/* emit empty set */
    if (symbol == SYM_RIGHTBRACKET)
    {
	nextsymbol();
	exprtype = typeuniversal;
    } else
    {
	exprtype = expression();
	if (!isin(exprtype, ENUMERABLES))
	    compiler_error(129, ERR_TYPE);
	if (symbol == SYM_DOUBLEDOT)
	{
	    nextsymbol();
	    exprtype2 = expression();
	    checktypes(exprtype2, exprtype);
	    emit(OP_ADDSETRANGE);
	} else
	    emit(OP_ADDSETELEMENT);
	while (symbol == SYM_COMMA)
	{
	    nextsymbol();
	    exprtype2 = expression();
	    checktypes(exprtype2, exprtype);
	    if (symbol == SYM_DOUBLEDOT)
	    {
		nextsymbol();
		exprtype2 = expression();
		checktypes(exprtype2, exprtype);
		emit(OP_ADDSETRANGE);
	    } else
		emit(OP_ADDSETELEMENT);
	}
	expect(SYM_RIGHTBRACKET);
    }
    return exprtype;
}

/************************************************************************/
/* Factor ::- VariableAccess. 						*/
/*	   | UnsignedConstant. 						*/
/*	   | FunctionDesignator. 					*/
/*	   | SetConstructor. 						*/
/*	   | "(" Expression ")". 					*/
/*	   | "not" Factor. 						*/
/*	   | ExistOne. 							*/
/* Term ::- Factor [ MultiplyingOperator Factor ]^0 . }			*/
/* MultiplyingOperator ::- "*" | "/" | "div" | "mod" | "and". }		*/
/* SimpleExpression ::- [ SignOperator ] Term [ AddingOperator Term ]^0.*/
/* AddingOperator   ::- "+" | "-" | "or". }				*/
/* Expression ::- SimpleExpression					*/
/*			[ RelationalOperator SimpleExpression ]. }	*/
/*	       | ModuleVariable RelationalOperator ModuleVariable. }	*/
/* RelationalOperator ::- "<" | "=" | ">" | "<=" | "<>" | ">=" | "in". 	*/
/************************************************************************/

static pointer NEAR 
factor(void)
{
    pointer type = 0, object;
    valuepointer value;
    short length;
    short counter;

    switch (symbol)
    {
    case SYM_INTEGER:
	type = constant(&value);
	emit(OP_CONSTANT, value->_Val_Int);
	break;
    case SYM_IDENTIFIER:
	object = find(argument);
	switch (object->kind)
	{
	case CONSTANTX:
	    type = constant(&value);
	    if (object->ident == _NIL)
		emit(OP_CONSTANT, 0);
	    else if (isin(type, SIMPLETYPES))
		emit(OP_CONSTANT, value->_Val_Int);
	    else if (isin(type, STRINGS))
	    {
		length = type->_Aray_UpperVal;
		for (counter = 0; counter < length; counter++)
		    emit(OP_CONSTANT, (int)(value->_Val_String[counter]));
	    }
	    break;
	case FIELD:
	case MODVAR:
	case VARIABLE:
	case VALUEPARAMETER:
	case VARPARAMETER:
	case INTERACTIONARG:
	    type = variable_access(FALSE, FALSE);
	    length = typelength(type);
	    if (type->kind != MODULETYPE)
		emit(OP_VALUE, length);
	    push(length - 1);
	    break;
	case PROCEDUR:
	case STANDARDFUNC:
	    type = function_statement(object);
	    break;
	default:
	    compiler_error(335, ERR_TYPE);
	}
	break;
    case SYM_LEFTPARENTHESIS:
	nextsymbol();
	type = expression();
	expect(SYM_RIGHTPARENTHESIS);
	break;
    case SYM_NOT:
	nextsymbol();
	type = expression();
	checktypes(type, typeboolean);
	emit(OP_NOT);
	break;
    case SYM_LEFTBRACKET:
	type = define(NOIDENTIFIER, SETTYPE);
	type->type = set_constructor();
	break;
    case SYM_GRAPHIC:
	type = constant(&value);
	if (isin(type, STRINGS))
	{
	    length = type->_Aray_UpperVal;
	    for (counter = 0; counter < length; counter++)
		emit(OP_CONSTANT, (short)(value->_Val_String[counter]));
	} else
	    emit(OP_CONSTANT, value->_Val_Int);
	break;
    case SYM_EXIST:
	existone();
	type = typeboolean;
	break;
    default:
	compiler_error(336, ERR_BADFACTOR);
	break;
    }
    return type;
}


static pointer NEAR 
term(void)
{
    enum symboltype operator;
    pointer type, type2, effectivetype;
    enum compatibility_class compatiblecase;

    type = factor();
    while (symbol == SYM_AND || symbol == SYM_ASTERISK || symbol == SYM_DIV || symbol == SYM_MOD)
    {
	operator = symbol;
	nextsymbol();
	type2 = factor();
	compatiblecase = compatibletypes(type, type2);
	if (GENCOMPATIBLE(compatiblecase))
	{
	    if (compatiblecase == COMPATIBLE && type == typeinteger)
	    {
		switch (operator)
		{
		case SYM_ASTERISK:
		    emit(OP_MULTIPLY);
		    break;
		case SYM_DIV:
		    emit(OP_DIV);
		    break;
		case SYM_MOD:
		    emit(OP_MODULO);
		    break;
		case SYM_AND:
		    compiler_error(337, ERR_TYPE);
		default:
		    break;
		}
	    } else if (type == typeboolean && compatiblecase == COMPATIBLE)
	    {
		checktypes(type, type2);
		if (operator == SYM_AND)
		    emit(OP_AND);
		else
		    compiler_error(338, ERR_TYPE);
	    } else if (compatiblecase == SETCOMPATIBLE)
		emit(OP_INTERSECTION);
	    else if (compatiblecase == SUBRANGECOMPATIBLE || compatiblecase == RANGECOMPATIBLE)
	    {
		effectivetype = type;
		if (compatiblecase == RANGECOMPATIBLE)
		    effectivetype = type->type;
		if (effectivetype == typeinteger)
		{
		    if (operator == SYM_ASTERISK)
			emit(OP_MULTIPLY);
		    else if (operator == SYM_DIV)
			emit(OP_DIV);
		    else if (operator == SYM_MOD)
			emit(OP_MODULO);
		    else
			compiler_error(339, ERR_TYPE);
		} else if (effectivetype == typeboolean)
		{
		    if (operator == SYM_AND)
			emit(OP_AND);
		    else
			compiler_error(340, ERR_TYPE);
		} else
		    compiler_error(341, ERR_TYPE);
	    } else
		compiler_error(342, ERR_TYPE);
	} else
	    compiler_error(343, ERR_TYPE);
    }
    return type;
}


static pointer NEAR 
simple_expression(void)
{
    enum symboltype operator;
    pointer type, type2, effectivetype;
    enum compatibility_class compatiblecase;

    if (symbol == SYM_MINUS || symbol == SYM_PLUS)
    {
	operator = symbol;
	nextsymbol();
	type = term();
	if (type == typeinteger)
	{
	    if (operator == SYM_MINUS)
		emit(OP_MINUS);
	    else
		compiler_error(344, ERR_TYPE);
	}
    } else
	type = term();
    while (symbol == SYM_MINUS || symbol == SYM_PLUS || symbol == SYM_OR)
    {
	operator = symbol;
	nextsymbol();
	type2 = term();
	compatiblecase = compatibletypes(type, type2);
	if (GENCOMPATIBLE(compatiblecase))
	{
	    if ((type == typeinteger || type->kind == SUBRANGETYPE)
		&& compatiblecase == COMPATIBLE)
	    {
		switch (operator)
		{
		case SYM_PLUS:
		    emit(OP_ADD);
		    break;
		case SYM_MINUS:
		    emit(OP_SUBTRACT);
		    break;
		case SYM_OR:
		    compiler_error(345, ERR_TYPE);
		    break;
		default:
		    break;
		}
	    } else if (type == typeboolean && compatiblecase == COMPATIBLE)
	    {
		if (operator == SYM_OR)
		    emit(OP_OR);
		else
		    compiler_error(346, ERR_TYPE);
	    } else if (compatiblecase == SETCOMPATIBLE)
	    {
		switch (operator)
		{
		case SYM_PLUS:
		    emit(OP_UNION);
		    break;
		case SYM_MINUS:
		    emit(OP_DIFFERENCE);
		    break;
		case SYM_OR:
		    compiler_error(347, ERR_TYPE);
		    break;
		default:
		    break;
		}
	    } else if (compatiblecase == RANGECOMPATIBLE || compatiblecase == SUBRANGECOMPATIBLE)
	    {
		effectivetype = type;
		if (compatiblecase == RANGECOMPATIBLE)
		    effectivetype = type->type;
		if (effectivetype == typeinteger)
		{
		    if (operator == SYM_PLUS)
			emit(OP_ADD);
		    else if (operator == SYM_MINUS)
			emit(OP_SUBTRACT);
		    else
			compiler_error(348, ERR_TYPE);
		} else if (effectivetype == typeboolean)
		{
		    if (operator == SYM_OR)
			emit(OP_OR);
		    else
			compiler_error(349, ERR_TYPE);
		} else
		    compiler_error(350, ERR_TYPE);
	    } else
		compiler_error(351, ERR_TYPE);
	} else
	    compiler_error(352, ERR_TYPE);
    }
    return type;
}


pointer 
expression(void)
{
    enum symboltype operator;
    pointer type, type2;
    short length;
    enum compatibility_class compatiblecase;

    type = simple_expression();
    switch (symbol)
    {
    case SYM_EQUAL:
    case SYM_NOTEQUAL:
    case SYM_GREATER:
    case SYM_NOTGREATER:
    case SYM_LESS:
    case SYM_NOTLESS:
    case SYM_IN:
	operator = symbol;
	nextsymbol();
	type2 = simple_expression();
	compatiblecase = compatibletypes(type, type2);
	if (GENCOMPATIBLE(compatiblecase))
	{
	    if (type->kind == POINTERTYPE)
	    {
		switch (operator)
		{
		case SYM_EQUAL:
		    emit(OP_EQUAL);
		    break;
		case SYM_NOTEQUAL:
		    emit(OP_NOTEQUAL);
		    break;
		default:
		    compiler_error(353, ERR_POINTEROP);
		}
	    } else if (isin(type, SIMPLETYPES))
	    {
		switch (operator)
		{
		case SYM_LESS:
		    emit(OP_LESS);
		    break;
		case SYM_EQUAL:
		    emit(OP_EQUAL);
		    break;
		case SYM_GREATER:
		    emit(OP_GREATER);
		    break;
		case SYM_NOTGREATER:
		    emit(OP_NOTGREATER);
		    break;
		case SYM_NOTEQUAL:
		    emit(OP_NOTEQUAL);
		    break;
		case SYM_NOTLESS:
		    emit(OP_NOTLESS);
		    break;
		case SYM_IN:
		    compiler_error(353, ERR_TYPE);
		    break;
		default:
		    break;
		}
	    } else if (isin(type, STRINGS))
	    {
		length = typelength(type);
		switch (operator)
		{
		case SYM_LESS:
		    emit(OP_STRINGCOMPARE, length, (int)(OP_LESS));
		    break;
		case SYM_EQUAL:
		    emit(OP_STRINGCOMPARE, length, (int)(OP_EQUAL));
		    break;
		case SYM_GREATER:
		    emit(OP_STRINGCOMPARE, length, (int)(OP_GREATER));
		    break;
		case SYM_NOTGREATER:
		    emit(OP_STRINGCOMPARE, length, (int)(OP_NOTGREATER));
		    break;
		case SYM_NOTEQUAL:
		    emit(OP_STRINGCOMPARE, length, (int)(OP_NOTEQUAL));
		    break;
		case SYM_NOTLESS:
		    emit(OP_STRINGCOMPARE, length, (int)(OP_NOTLESS));
		    break;
		case SYM_IN:
		    compiler_error(354, ERR_TYPE);
		    break;
		default:
		    break;
		}
		pop(length * 2 - 1);
	    } else if (compatiblecase == SETCOMPATIBLE)
	    {
		switch (operator)
		{
		case SYM_EQUAL:
		    emit(OP_SETEQUAL);
		    break;
		case SYM_NOTEQUAL:
		    emit(OP_SETEQUAL);
		    emit(OP_NOT);
		    break;
		case SYM_NOTGREATER:
		    emit(OP_SETINCLUSION);
		    break;
		case SYM_NOTLESS:
		    emit(OP_SETINCLUSIONTOO);
		    break;
		default:
		    compiler_error(355, ERR_TYPE);
		}
	    } else
		compiler_error(356, ERR_TYPE);
	} else if (compatiblecase == ELTOFCOMPATIBLE)
	{
	    if (operator == SYM_IN)
		emit(OP_IN);
	    else
		compiler_error(357, ERR_TYPE);
	}
	type = typeboolean;
	break;
    default:
	break;
    }
    return type;
}
