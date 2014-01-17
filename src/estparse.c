/* estparse.c - estelle construct parser */

#include "symbol.h"
#include "compiler.h"

/********************************************************************
Revision notes:

        89-6-12: trans_groups is probably wrong, as variables declared
                within transition blocks are local to the block, and
                so it is meaningless to accumulate their lengths over
                multiple transistion blocks within the same module body.

	89-6-13: the above comment is possibly sorted out, as the
		stack layout of transitions must change, to:

				temporaries
			     s->===========
				local trans
				variables
				===========
			     b->   AR	   ---
				===========  |
				when clause  |
				parameters   |
				===========  |
				local module |
				variables    |
				===========  |
				   AR	   <-/
				===========
				module init
				parameters
				===========
				exported
				variables
			     0  ===========

		the first part (exported variables and init params) is
		created upon module initialisation), as is the stack space
		for local module variables. The when clauses parameters are
		stacked and a new activation created upon execution of an
		enabled transition. The address used at the top level of
		the transition body is:

			type		relative level   sign of displ
			==============================================
			exported vars	   1		   -ve
			init params	   1		   -ve
			module vars	   1		   +ve
			when params	   0		   -ve
			trans vars	   0		   +ve

		The necessary changes must be made. modvar addressing and
		IP addressing remain the same. module vars and trans vars
		can be handled by variable_addressing. init params and when
		params can be handled by parameter_addressing.

	89-9-19 Jaque had a really bad way of dealing with module headers
		and bodies: he would process the header, group all IPs in
		the symbol table for the header into a single chunk by
		rearranging the stab linked list, and set a pointer to
		this. When in a module body, he would copy the appropriate
		number of stab entries from this pointer. This had several
		nasties:
			- rearranging a symbol table where ordering
				should be unimportant
			- he dealt with exported vars, IPs, etc all
				separately (but in the same way)
			- he duplicated symbol table entries unnecessarily
			- he basically did not deal with arrays of IPs
				(more precisely, his code was bugged so
				that it only worked for non-arrays).

		I have now replaced this whole mess; this results in about
		25 lines less of code, several fields can be dropped from
		module header symbol table entries, no symbol table rearrangement
		is needed, and no entries are copied or duplicated. What
		happens now is that the header symbol table is prepended
		onto the body symbol table by just setting a pointer (_Mod_LastEntry).
		Thus every body for the same header shares the same symbol table
		entries for the header part.
	Further Logs are on PVCS.

*********************************************************************/

/********************/
/* Pseudo-functions */
/********************/

#define add2set(set,elt) 		set[(elt)/16] |= 1<<((elt)%16)
#define isinset(set,elt)		(set[(elt)/16] & (1<<((elt)%16)))
#define constant_ident()		gen_ident(CONSTANTX)
#define modvar_ident()			gen_ident(MODVAR)
#define channel_ident()			gen_ident(CHANNELTYPE)
#define role_ident()			gen_ident(ROLETYPE)
#define IP_ident()			gen_ident(IP_)
#define interaction_argument_ident()	gen_ident(INTERACTIONARG)

static int
    default_discipline, default_timeunit;

/************************************************/
/* The clause stack, and clause variables.	*/
/************************************************/

static enum symboltype
    clause_stack[7];		/* 7 different clause types	 */
static short clause_TOS = 0;	/* TOS index 			 */

static short prov_label = -1,	/* Provided clause label	 */
    prov_first = -1;		/* Used in OTHERWISE type.	 */

static short from1 = -1,	/* Labels associated with set	 */
    from2 = -1,			/* of states for FROM clause:	 */
    from3 = -1,			/* each 16-bit word of the set	 */
    from4 = -1,			/* has its own label.		 */
    from_ident = -1;		/* From clause ID number	 */
static setconstanttype
    fromset;			/* Set of states		 */

static short to_state = -1,	/* To clause state number	 */
    to_ident = -1;		/* To clause state ID number	 */

static short delay_label = -1;	/* Label of delay clause	 */

static short priority_val = -1;	/* Proiority value constant	 */

static short when_label = -1,	/* Label of start of when clause */
    when_len = 0;		/* Length of when clause arg	 */
static pointer when_IP = NULL;

/******************************************************/
/* state deadlock and reachability checking variables */
/******************************************************/

static setconstanttype used_in_from, used_in_to;
static BOOLEAN from_any;

/*********************************/
/* Prototypes of local functions */
/*********************************/

static pointer NEAR state_ident(void);
static BOOLEAN NEAR body_definition(short extIPcnt, processclass class, short body_id, short head_id);
static void NEAR provided_clause(short trans_num);
static void NEAR to_clause(void);
static void NEAR when_clause(void);
static void NEAR clause_group(BOOLEAN init_type, short trans_num);
static pointer NEAR interaction_definition(short Qtyp);
static void NEAR 
interaction_group(pointer * channel, grouppointer * group,
		  short *rolegroup);
static void NEAR IP_group(pointer * type, short *number);
static short NEAR IP_declaration(void);
static void NEAR IP_indexed_selector(pointer * type);
static short NEAR queue_discipline(void);
static void NEAR stateset_definition(void);

static void NEAR trans_block(short transname, short translabel, short varlabel, short tmplabel, short startscope);
static void NEAR trans_declaration_part(short trans_lbl, short transcnt);
static short NEAR trans_group(BOOLEAN init_type, short trans_num, short *next_lbl);

/************************************************************************/

static void NEAR 
    table_addressing(short cnt, short place, enum class kind)
    {
	pointer var = block[blocklevel].lastobject;
	if  (cnt > 0)
	{
	    while (var->kind != kind)
		var = var->previous;
	    while (cnt > 0)
	    {
		short tmp = typelength(var->type);
		    cnt -= tmp;
		    place -= tmp;
		    var->place = place;
		do
		{
		    var = var->previous;
		} while (var && cnt > 0 && var->kind != kind);
	    }
	}
    }

static void NEAR 
dump_scope(short start)
{
    short i;
    for (i = start; i <= blocklevel; i++)
	if (block[i].lastobject)
	    emit(OP_SCOPE, (short)i, (short)(PTR_TO_LONG(block[i].lastobject) - PTR_TO_LONG(locspace)));
	else
	    emit(OP_SCOPE, (short)i, -1);
}

/****************************************/
/*	Clause stack operations		*/
/****************************************/

static short NEAR 
pop_clause_stack(void)
{
    if (clause_TOS)
    {
	switch (clause_stack[--clause_TOS])
	{
	    case SYM_FROM:from1 = from2 = from3 = from4 = from_ident = -1;
	    break;
	case SYM_TO:
	    to_state = to_ident = -1;
	    break;
	case SYM_PROVIDED:
	    prov_label = -1;
	    break;
	case SYM_WHEN:
	    when_label = -1;
	    when_len = 0;
	    when_IP = NULL;
	    break;
	case SYM_ANY:
	    break;
	case SYM_PRIORITY:
	    priority_val = -1;
	    break;
	case SYM_DELAY:
	    delay_label = -1;
	    break;
	default:
	    break;
	}
    }
    return clause_TOS;
}

static void NEAR 
clear_clause_stack(void)
{
    while (pop_clause_stack());
    prov_first = -1;
}


static void NEAR 
check_implicit_when(void)
{
    short i;
    for (i = 0; i < clause_TOS; i++)
    {
	if (clause_stack[i] == SYM_WHEN)	/* Got implicit when */
	{
	    if (when_len > 0)
	    {
		/*
		 * Make symbol table definitions for the WHEN IP arguments.
		 */
		short tmplen = when_len;
		pointer lastarg = when_IP->_Intr_LastArg;
		while (tmplen > 0)
		{
		    tmplen -= typelength(lastarg->type);
		    redefine(lastarg);
		    lastarg = lastarg->previous;
		}
	    }
	    break;
	}
    }
}

static void NEAR 
    check_clause_stack(enum symboltype cl_type, short *thisgrp)
    {
	short i;
	BOOLEAN done = FALSE;
	for (i = *thisgrp; i < clause_TOS; i++)	/* Check for DUPS in new
						 * clauses */
	{
	    if (clause_stack[i] == cl_type)
		compiler_error(222, ERR_DUPCLAUSE);
	}
	for (i = 0; i < *thisgrp; i++)	/* Look at old clauses */
	{
	    if (clause_stack[i] == cl_type)	/* Dup found? */
	    {
		if (*thisgrp != clause_TOS)	/* Badly nested clauses */
		    compiler_error(226, ERR_CLAUSENEST);
		else
		{
		    /* Pop stack. */
		    if (i != clause_TOS)
			while (i != pop_clause_stack());
		    clause_TOS = *thisgrp = i + 1;
		    check_implicit_when();
		}
		done = TRUE;
		break;
	    }
	}
	if (!done)		/* No dups found; make new entry */
	{
	    clause_stack[clause_TOS++] = cl_type;
	    check_implicit_when();
	}
    }

/************************************************************************/
/* SystemClass ::- "systemprocess" | "systemactivity".			*/
/* DefaultOptions ::- "default" QueueDiscipline ";". 			*/
/* Specification ::- "specification" Identifier [ SystemClass ] ";"	*/
/*		    [ DefaultOptions ] [ TimeOptions ]			*/
/*			BodyDefinition "end" "."			*/
/************************************************************************/

void 
specification(void)
{
    processclass class;
    short spec_id;
    expect(SYM_SPECIFICATION);
    spec_id = define_nextident(BODY)->ident;	/* Need stab entry */
    if (symbol == SYM_SYSTEMACTIVITY)
    {
	class = CLASS_SYSACTIVITY;
	nextsymbol();
    } else if (symbol == SYM_SYSTEMPROCESS)
    {
	class = CLASS_SYSPROCESS;
	nextsymbol();
    } else
	class = CLASS_UNATTRIB;
    expect(SYM_SEMICOLON);
    if (symbol == SYM_DEFAULT)
    {
	nextsymbol();
	default_discipline = queue_discipline();
	expect(SYM_SEMICOLON);
    } else
	default_discipline = -1;
    if (symbol == SYM_TIMESCALE)
    {
	nextsymbol();
	default_timeunit = expect_ident();
	/*
	 * NB - I assume a contiguous ordering of values of the timeunit
	 * standard identifiers in the next check..
	 */
	if (default_timeunit < _HOURS || default_timeunit > _MICROSECONDS)
	    compiler_error(218, ERR_TIME);
	expect(SYM_SEMICOLON);
    } else
	default_timeunit = -1;
    new_block(TRUE);
    if (body_definition(0, class, spec_id, 0))
	/* If spec body has a trans part, it must be attributed... */
	if (class == CLASS_UNATTRIB)
	    compiler_error(219, ERR_SYSTEMCLASS);
    exitblock();
    expect(SYM_END);
    expect(SYM_PERIOD);
}

/************************************************************************/
/* InitializationGroups ::- InitializationTrans [ InitializationGroups].*/
/* InitializationPart ::- [ "initialize" ( InitializationGroups		*/
/*					| TransitionBlock ";" ) ]^1.	*/
/* BodyDefinition ::- DeclarationPart					*/
/*		     InitializationPart					*/
/*		     TransitionDeclarationPart.				*/
/* ModuleBodyDefinition ::- "body" Identifier "for"			*/
/*		HeaderIdentifier ";"					*/
/*		   ( BodyDefinition "end" ";" | "external" ";" ).	*/
/* Class ::- "systemprocess" | "systemactivity" | "process" | "activity".*/
/* ModuleHeaderDefinition ::- "module" Identifier [ Class ]		*/
/*		     [ "(" ParameterList ")" ] ";"			*/
/*		     [ "ip" [ InteractionPointDeclaration ";" ]^1 ]	*/
/*		     [ "export" [ ExportedVariableDeclaration ";"]^1 ]	*/
/*		     "end" ";".						*/
/* StructuredModVarType ::- "array" "[" IndexTypeList "]"		*/
/*					"of" HeaderIdentifier.		*/
/* ModVarTypeDenoter ::- HeaderIdentifier | StructuredModVarType. 	*/
/* ModVarGroup ::- Identifier ModVarGroupTail.				*/
/* ModVarGroupTail ::- "," ModVarGroup | ":" ModVarTypeDenoter. 	*/
/* ModuleVariableDeclaration ::- IdentifierList ":" ModVarTypeDenoter.	*/
/* ModuleVariableDeclarationPart ::- "modvar"				*/
/*				[ModuleVariableDeclaration ";"]^1.	*/
/************************************************************************/

static short NEAR 
get_stateident(short statenum)
{
    pointer tmp = block[blocklevel].lastobject;
    while (tmp)
    {
	if (tmp->kind == CONSTANTX)
	    if (tmp->type->kind == STATEX)
		if (tmp->_Const_Int == statenum)
		    break;
	tmp = tmp->previous;
    }
    return tmp->ident;
}

static BOOLEAN NEAR 
body_definition(short extIPcnt, processclass class, short modbod_id, short modhead_id)
{
    short varl = new_label(), mdvarcnt = new_label(), initcnt = new_label(), transcnt = new_label(),
        intIPcnt = new_label(), ipdeflabel = new_label(), initlabel = new_label(), translabel = new_label(),
        scopelabel = new_label(), tgrouplabel = new_label();
    short old_variable_scope_base = variable_scope_base, i, numstates;
    BOOLEAN rtn, has_common_ips;
    pointer obj;

    variable_scope_base = blocklevel;	/* Hide variables below this level */
    emit(OP_MODULE, modbod_id, modhead_id, class, varl, mdvarcnt, initcnt, transcnt,
	 intIPcnt, extIPcnt, initlabel, translabel, ipdeflabel, scopelabel, tgrouplabel);
    emit(OP_DEFARG, varl, declaration_part(mdvarcnt, intIPcnt, extIPcnt, &numstates));

    for (i = 0; i < SETSIZE; i++)
	used_in_from[i] = used_in_to[i] = 0;

    /* Specify which IPs are common queue */
    has_common_ips = FALSE;
    obj = block[blocklevel].lastobject;
    while (obj != NULL)
    {
	if (obj->kind == IP_)
	{
	    short size, start;
	    pointer type = obj->type;
	    size = typelength(type);
	    start = obj->place;
	    while (type && type->kind != IPTYPE)
		type = type->type;
	    if (type->_IPTyp_Disc == 0)
	    {
		if (has_common_ips == FALSE)
		{
		    has_common_ips = TRUE;
		    emit(OP_DEFADDR, ipdeflabel);
		}
		emit(OP_DEFIPS, start, size);
	    }
	}
	obj = obj->previous;
    }
    if (has_common_ips == FALSE)
	emit(OP_DEFARG, ipdeflabel, -1);
    emit(OP_DEFADDR, scopelabel);
    dump_scope(0);
    if (symbol == SYM_INITIALIZE)
    {
	nextsymbol();
	i = trans_group(TRUE, 0, &initlabel);
    } else
	i = 0;
    emit(OP_DEFARG, initcnt, i);
    from_any = FALSE;
    if (symbol == SYM_TRANS)
    {
	tgroup = 0;
	trans_declaration_part(translabel, transcnt);
	if (tgroup == 99)
	    tgroup = 0;		/* Second group was empty */
	emit(OP_DEFARG, tgrouplabel, tgroup);
	rtn = TRUE;
    } else
    {
	emit(OP_DEFARG, transcnt, 0);
	rtn = FALSE;
    }

    /* Check for unreachable states and deadlock states */

    for (i = 0; i < numstates; i++)
	if (!isinset(used_in_to, i))
	    warn(WARN_UNREACHABLE, get_stateident(i));
    if (!from_any)		/* possibly have deadlock states */
    {
	for (i = 0; i < numstates; i++)
	    if (isinset(used_in_to, i) && !isinset(used_in_from, i))
	    {
		warn(WARN_DEADLOCK, get_stateident(i));
	    }
    }
    variable_scope_base = old_variable_scope_base;
    return rtn;
}

void 
modulebody_definition(void)
{
    pointer body, header;
    short body_label;

    expect(SYM_BODY);
    body = define_nextident(BODY);
    emit(OP_DEFADDR, body_label = new_label());
    new_block(TRUE);
    expect(SYM_FOR);
    body->_Body_Header = header = header_ident();
    body->place = body_label;
    expect(SYM_SEMICOLON);

    /* Link in the symbol table entries for the header */

    block[blocklevel].lastobject = header->_Mod_LastEntry;

    if (symbol == SYM_EXTERNAL)
	compiler_error(434, ERR_EXTERNAL);
    else
    {
	(void)body_definition(header->_Mod_NumIPs, header->_Mod_Class, body->ident, header->ident);
	expect(SYM_END);
    }
    expect(SYM_SEMICOLON);
    exitblock();
}


void 
moduleheader_definition(void)
{
    short length, number;
    pointer header;
    processclass modclass;

    expect(SYM_MODULE);
    header = define_nextident(MODULETYPE);
    header->_Mod = (modulepointer) EC_malloc(sizeof(struct modulerecord));

    new_block(TRUE);

    switch (symbol)
    {
    case SYM_SYSTEMPROCESS:
	modclass = CLASS_SYSPROCESS;
	break;
    case SYM_SYSTEMACTIVITY:
	modclass = CLASS_SYSACTIVITY;
	break;
    case SYM_PROCESS:
	modclass = CLASS_PROCESS;
	break;
    case SYM_ACTIVITY:
	modclass = CLASS_ACTIVITY;
	break;
    default:
	modclass = CLASS_UNATTRIB;
	break;
    }
    if (modclass != CLASS_UNATTRIB)
	nextsymbol();
    header->_Mod_Class = modclass;
    if (symbol == SYM_LEFTPARENTHESIS)
    {
	nextsymbol();
	header->_Mod_ParmLen = parameter_list(VALUEPARAMETER, &header->_Mod_LastPar);
	expect(SYM_RIGHTPARENTHESIS);
	parameter_addressing(header->_Mod_ParmLen, header->_Mod_LastPar, 0);
    } else
    {
	header->_Mod_LastPar = NULL;
	header->_Mod_ParmLen = 0;
    }
    expect(SYM_SEMICOLON);


    number = 0;
    if (symbol == SYM_IP)
    {
	nextsymbol();
	while (symbol == SYM_IDENTIFIER)
	{
	    number += IP_declaration();
	    expect(SYM_SEMICOLON);
	}
	if (number)
	    table_addressing(number, number, IP_);
	else
	    compiler_error(435, ERR_NOIDENT);
    }
    header->_Mod_NumIPs = number;
    length = 0;
    if (symbol == SYM_EXPORT)
    {
	short displ;
	pointer lastexvar;
	nextsymbol();
	while (symbol == SYM_IDENTIFIER)
	{
	    length += variable_declaration(&lastexvar);
	    expect(SYM_SEMICOLON);
	}
	header->_Mod_ExVars = length;
	displ = -(length + header->_Mod_ParmLen);
	while (length)
	{
	    short len;
	    lastexvar->place = displ;
	    lastexvar->_Var_Level = blocklevel;
	    lastexvar->_Var_IsLoop = FALSE;
	    lastexvar->_Var_IsExp = TRUE;
	    len = typelength(lastexvar->type);
	    length -= len;
	    displ += len;
	    do
	    {
		lastexvar = lastexvar->previous;
	    } while (lastexvar && lastexvar->ident == NOIDENTIFIER);
	}
    } else
	header->_Mod_ExVars = 0;
    header->_Mod_LastEntry = block[blocklevel].lastobject;
    expect(SYM_END);
    expect(SYM_SEMICOLON);

    exitblock();
}

void 
modvar_declaration_part(short modvarcnt)
{
    pointer type, lastmodvar;
    short size = 0;
    expect(SYM_MODVAR);
    do
    {
	short thistime;
	ident_list(&lastmodvar, &thistime, MODVAR);
	if (symbol == SYM_ARRAY)
	{
	    pointer tmp = array_specification(&type);
	    tmp->type = header_ident();
	} else
	    type = header_ident();
	expect(SYM_SEMICOLON);
	assign_type(lastmodvar, thistime, type);
	size += thistime * typelength(type);
    } while (symbol == SYM_IDENTIFIER);
    emit(OP_DEFARG, modvarcnt, size);
    table_addressing(size, size, MODVAR);	/* more efficient if I use
						 * lastmodvar */
}

pointer 
module_variable(BOOLEAN include_symbolic_info)
{
    pointer type, modvarx = modvar_ident();
    if (modvarx->place >= 0)
	emit(OP_CONSTANT, modvarx->place);
    /* else we have a module domain variable from EXIST etc ... */
    else
    {
	emit(OP_VARIABLE, blocklevel - modvarx->_Var_Level, modvarx->place);
	emit(OP_VALUE, 1);	/* push(0); */
    }
    type = modvarx->type;
    if (symbol == SYM_LEFTBRACKET)
	indexed_selector(&type);
    if (include_symbolic_info && symbol != SYM_PERIOD)
	/*
	 * The check for SYM_PERIOD is to distinguish module variable
	 * assignments from exported variable assignments. See
	 * variable_access
	 */
	emit(OP_CONSTANT, modvarx->ident);
    return type;
}


/************************************************************************/
/* TransitionName ::- "name" Identifier ":".				*/
/* TransitionBlock ::- ConstantDefinitionPart				*/
/*		      TypeDefinitionPart				*/
/*		      VariableDeclarationPart				*/
/*		      ProcedureAndFunctionDeclarationPart		*/
/*		      [ TransitionName ] CompoundStatement.		*/
/* TransitionDeclaration  ::- "trans" TransitionGroups			*/
/*				| TransitionBlock ";".			*/
/* TransitionDeclarationPart ::- [ TransitionDeclaration ]^0.		*/
/* TransitionGroup ::- Clauses TransitionBlock ";".			*/
/* TransitionGroups ::- TransitionGroup [ TransitionGroups ].		*/
/* InitializationClause ::- ToClause | ProvidedClause.			*/
/* InitializationClauses ::- InitializationClause			*/
/*				[ InitializationClauses ].		*/
/* InitializationTrans ::- InitializationClauses TransitionBlock ";".	*/
/************************************************************************/

static void NEAR 
trans_block(short transname, short transblock, short varlabel, short tmplabel, short startscope)
{
    short varlen = 0;
    if (symbol == SYM_CONST)
	constant_definition_part();
    if (symbol == SYM_TYPE)
	type_definition_part();
    if (symbol == SYM_VAR)
	varlen = variable_declaration_part(0);
    if (symbol == SYM_PROCEDURE || symbol == SYM_FUNCTION)
	proc_and_func_decl_part();
    if (symbol == SYM_NAME)
    {
	nextsymbol();
	emit(OP_DEFARG, transname, expect_ident());
	expect(SYM_COLON);
    } else
	emit(OP_DEFARG, transname, -1);
    emit(OP_DEFADDR, transblock);
    dump_scope(startscope);
    compound_statement();
    emit(OP_DEFARG, tmplabel, block[blocklevel].maxtemp);
    emit(OP_DEFARG, varlabel, varlen);
    emit(OP_ENDTRANS);
}

static short NEAR 
trans_group(BOOLEAN init_type, short trans_num, short *trans_label)
{
    short num_trans = 0;

    clear_clause_stack();
    prov_first = trans_num;
    do
    {
	short tr_name = new_label(), tr_block = new_label(), tr_vars = new_label(),
	    tr_tmps = new_label();
	short scopestart;
	if (tgroup == 99)
	    tgroup = trans_num;	/* Split at this point? */
	new_block(TRUE);
	num_trans++;
	scopestart = blocklevel;
	clause_group(init_type, trans_num++ /* for OTHERWISE */ );
	emit(OP_DEFADDR, *trans_label);
	*trans_label = new_label();
	emit(OP_TRANS, tr_name, from1, from2, from3, from4, from_ident,
	      /* 7 */ to_state, to_ident, priority_val, prov_label,
	      /* 11*/ when_label, delay_label, *trans_label, when_len,
	      /* 15*/ tr_vars, tr_tmps, tr_block);
	trans_block(tr_name, tr_block, tr_vars, tr_tmps, scopestart);
	expect(SYM_SEMICOLON);
	exitblock();
    } while (symbol != SYM_TRANS && symbol != SYM_END);
    return num_trans;
}

static void NEAR 
trans_declaration_part(short trans_lbl, short transcnt)
{
    short cnt = 0;

    while (symbol == SYM_TRANS)
    {
	nextsymbol();
	cnt += trans_group(FALSE, cnt, &trans_lbl);
	if (cnt > MAXTRANS)
	    compiler_error(206, ERR_MAXTRANS);
    }
    emit(OP_DEFARG, trans_lbl, -1);
    emit(OP_DEFARG, transcnt, cnt);
    if (cnt > MAXTRANS)
	compiler_error(206, ERR_MAXTRANS);
}

/************************************************************************/
/* AnyClause ::- "any" IdentifierList ":" OrdinalType			*/
/*		[ ";" IdentifierList ":" OrdinalType ]^0 "do".		*/
/* DelayClause ::- "delay" "(" ( Expression "," Expression		*/
/* 			| Expression "," "*" | Expression ) ")" .	*/
/* FromElement ::- StateIdentifier | StateSetIdentifier.		*/
/* FromList ::- FromElement [ "," FromElement ]^0.			*/
/* FromClause ::- "from" FromList.					*/
/* PriorityConstant ::- UnsignedInteger | ConstantIdentifier.		*/
/* PriorityClause ::- "priority" PriorityConstant.			*/
/* ProvidedClause ::- "provided" ( BooleanExpression | "otherwise" ).	*/
/* ToElement ::- "same" | StateIdentifier.				*/
/* ToClause ::- "to" ToElement.						*/
/* InteractionArgumentList ::- InteractionArgumentIdentifier		*/
/*			      [ "," InteractionArgumentIdentifier ]^0 .	*/
/* WhenIpReference ::- InteractionPointIdentifier [ IpIndexedSelector ]. */
/* WhenClause ::- "when" WhenIpReference "." InteractionIdentifier	*/
/*		 [ "(" InteractionArgumentList ")" ].			*/
/* Clauses ::- Clause Clauses.						*/
/* ClauseGroup ::- [ ProvidedClause ]					*/
/* ClauseGroup	! [ FromClause ]					*/
/* ClauseGroup	! [ ToClause ]						*/
/* ClauseGroup	! [ AnyClause ]						*/
/* ClauseGroup	! [ DelayClause ]					*/
/* ClauseGroup	! [ WhenClause ]					*/
/* ClauseGroup	! [ PriorityClause ].					*/
/* Where a1!a2!...!a7 means all permutations of a1..a7.			*/
/* Note: ISO/DP 9074 in clause 7.5.2.4 in an informal description	*/
/*	 does, however, apart from giving restrictions, also gives a	*/
/*	 description of this construct which does not agree with the	*/
/*	 syntactic rule.						*/
/************************************************************************/


static void NEAR 
provided_clause(short trans_num)
{
    /* prov_first holds the first in the set */

    expect(SYM_PROVIDED);
    emit(OP_DEFADDR, prov_label = new_label());
    if (symbol == SYM_OTHERWISE)
    {
	nextsymbol();
	if (prov_first == -1)
	    emit(OP_CONSTANT, 1);
	else
	    emit(OP_OTHERWISE, prov_first, trans_num - 1);
	prov_first = -1;
    } else
    {
	if (prov_first == -1)
	    prov_first = trans_num;
	checktypes(expression(), typeboolean);
    }
    emit(OP_ENDCLAUSE);
}

static void NEAR 
from_clause(void)
{
    pointer object;
    BOOLEAN has_unique_fromID = TRUE, has_unique_fromstate = TRUE;
    short i;

    for (i = 0; i < SETSIZE; i++)
	fromset[i] = 0;
    from_ident = new_label();
    do
    {
	if (symbol == SYM_COMMA)
	{
	    has_unique_fromstate = has_unique_fromID = FALSE;
	    emit(OP_DEFARG, from_ident, 0);	/* Can't cope */
	}
	nextsymbol();		/* skip FROM or comma */
	object = find(expect_ident());
	if (object->kind == STATESETX)
	{
	    for (i = 0; i < SETSIZE; i++)
		fromset[i] |= (*(object->_StSet_Val->_Const_Set))[i];
	    has_unique_fromstate = FALSE;
	} else if (object->kind == CONSTANTX)
	    if (object->type->kind != STATEX)
		compiler_error(205, ERR_TYPE);
	    else
		add2set(fromset, object->_Const_Int);
	else
	    compiler_error(206, ERR_TYPE);
    } while (symbol == SYM_COMMA);
    if (has_unique_fromID)
    {
	short fromID = object->ident;
	if (!has_unique_fromstate)
	    fromID = -fromID;
	emit(OP_DEFARG, from_ident, fromID);
    }
    emit(OP_DEFARG, from1 = new_label(), fromset[0]);
    emit(OP_DEFARG, from2 = new_label(), fromset[1]);
    emit(OP_DEFARG, from3 = new_label(), fromset[2]);
    emit(OP_DEFARG, from4 = new_label(), fromset[3]);
}

static void NEAR 
to_clause(void)
{
    pointer to_state_ptr;
    expect(SYM_TO);
    if (symbol == SYM_SAME)
    {
	nextsymbol();
	to_ident = 0;
	to_state = -1;
    } else
    {
	to_state_ptr = state_ident();
	to_state = to_state_ptr->_Const_Int;
	to_ident = to_state_ptr->ident;
	add2set(used_in_to, to_state);
    }
}

static void NEAR 
delay_clause(void)
{
    nextsymbol();
    expect(SYM_LEFTPARENTHESIS);
    emit(OP_DEFADDR, delay_label = new_label());
    checktypes(expression(), typeinteger);
    if (symbol == SYM_COMMA)
    {
	nextsymbol();
	if (symbol == SYM_ASTERISK)
	{
	    nextsymbol();
	    emit(OP_CONSTANT, -1);
	} else
	    checktypes(expression(), typeinteger);
    } else
	emit(OP_COPY);
    emit(OP_DELAY, distribution_type, dist_val);
    emit(OP_ENDCLAUSE);
    expect(SYM_RIGHTPARENTHESIS);
}


static void NEAR 
interaction_argumentlist(pointer lastarg)
{
    pointer argumentx;
    if (lastarg->previous != NULL)
    {
	interaction_argumentlist(lastarg->previous);
	expect(SYM_COMMA);
    }
    argumentx = interaction_argument_ident();
    checktypes(argumentx->type, lastarg->type);
}

static void NEAR 
when_clause(void)
{
    pointer ipx, interactionx = 0, channel, role, lastarg;
    short ident, tmplen;
    grouppointer nextinteraction = 0;
    BOOLEAN found;		/* Check first few lines carefully! */

    expect(SYM_WHEN);
    emit(OP_DEFADDR, when_label = new_label());
    ipx = IP_ident();
    emit(OP_CONSTANT, ipx->place);
    ipx = ipx->type;
    if (symbol == SYM_LEFTBRACKET)
	IP_indexed_selector(&ipx);
    expect(SYM_PERIOD);
    ident = expect_ident();
    channel = ipx->_IPTyp_Chan;
    role = ipx->_IPTyp_Role;
    if (role == channel->_Chan_User)
	nextinteraction = channel->_Chan_PInts;
    else if (role == channel->_Chan_Prov)
	nextinteraction = channel->_Chan_UInts;
    else
	compiler_error(999, ERR_WRONGROLE);
    found = FALSE;
    while (nextinteraction != NULL && !found)
    {
	if (ident == nextinteraction->interaction->ident)
	    found = TRUE;
	else
	    nextinteraction = nextinteraction->next;
    }
    if (!found)
	nextinteraction = channel->_Chan_UPInts;
    while (nextinteraction != NULL && !found)
    {
	if (ident == nextinteraction->interaction->ident)
	    found = TRUE;
	else
	    nextinteraction = nextinteraction->next;
    }
    if (!found)
	undef_ident_err(203, ID_NameStore + ID_MapTable[ident]);
    else
	when_IP = interactionx = nextinteraction->interaction;
    interactionx->_Intr_HasWhen = TRUE;
    tmplen = when_len = interactionx->_Intr_ArgLen;
    lastarg = interactionx->_Intr_LastArg;
    while (tmplen > 0)
    {
	tmplen -= typelength(lastarg->type);
	redefine(lastarg);
	lastarg = lastarg->previous;
    }
    if (symbol == SYM_LEFTPARENTHESIS)
    {
	if (interactionx->_Intr_LastArg != NULL)
	{
	    nextsymbol();
	    interaction_argumentlist(interactionx->_Intr_LastArg);
	    expect(SYM_RIGHTPARENTHESIS);
	} else
	    compiler_error(204, ERR_NOIDENT);
    }
    emit(OP_WHEN, ident, when_len);
    emit(OP_ENDCLAUSE);
}

static void NEAR 
clause_group(BOOLEAN init_type, short trans_num)
{
    short i, thisgrp_TOS = clause_TOS;
    BOOLEAN is_clause = TRUE, has_from_clause = FALSE;

    while (TRUE)
    {
	switch (symbol)
	{
	case SYM_PROVIDED:
	    check_clause_stack(SYM_PROVIDED, &thisgrp_TOS);
	    provided_clause(trans_num);
	    break;
	case SYM_FROM:
	    if (init_type)
		compiler_error(480, ERR_BADINITTRANS);
	    check_clause_stack(SYM_FROM, &thisgrp_TOS);
	    has_from_clause = TRUE;
	    from_clause();
	    break;
	case SYM_TO:
	    check_clause_stack(SYM_TO, &thisgrp_TOS);
	    to_clause();
	    break;
	case SYM_DELAY:
	    if (init_type)
		compiler_error(481, ERR_BADINITTRANS);
	    check_clause_stack(SYM_DELAY, &thisgrp_TOS);
	    delay_clause();
	    break;
	case SYM_WHEN:
	    if (init_type)
		compiler_error(482, ERR_BADINITTRANS);
	    check_clause_stack(SYM_WHEN, &thisgrp_TOS);
	    when_clause();
	    break;
	case SYM_PRIORITY:
	    if (init_type)
		compiler_error(483, ERR_BADINITTRANS);
	    check_clause_stack(SYM_PRIORITY, &thisgrp_TOS);
	    nextsymbol();
	    if (symbol == SYM_INTEGER)
	    {
		priority_val = argument;
		nextsymbol();
	    } else
		priority_val = constant_ident()->_Const_Int;
	    break;
	case SYM_ANY:
	    compiler_error(478, ERR_ANY);
	default:
	    is_clause = FALSE;
	    break;
	}
	if (!is_clause)
	    break;
    }

    if (!has_from_clause)
	from_any = TRUE;	/* Satisfied for any state */
    else
	for (i = 0; i < SETSIZE; i++)
	    used_in_from[i] |= fromset[i];
}


/************************************************************************/
/* RoleList ::- Identifier "," Identifier.				*/
/* ChannelHeading ::- "channel" Identifier "(" RoleList ")" ";".	*/
/* ChannelBlock ::- [ InteractionGroup ]^1.				*/
/* ChannelDefinition ::- ChannelHeading ChannelBlock.			*/
/************************************************************************/

void 
channel_definition(void)
{
    short rolegroup;
    pointer role, role2, channel;
    grouppointer groupx;
    expect(SYM_CHANNEL);
    channel = define_nextident(CHANNELTYPE);
    new_block(TRUE);
    expect(SYM_LEFTPARENTHESIS);
    role = define_nextident(ROLETYPE);
    expect(SYM_COMMA);
    role2 = define_nextident(ROLETYPE);
    channel->_Chan = (channelpointer) EC_malloc(sizeof(struct channelrecord));
    channel->_Chan_QType = chanQtype;
    chanQtype = FIFO_Q;
    channel->_Chan_User = role;
    channel->_Chan_Prov = role2;
    channel->_Chan_UInts = channel->_Chan_PInts = channel->_Chan_UPInts = NULL;
    expect(SYM_RIGHTPARENTHESIS);
    expect(SYM_SEMICOLON);
    interaction_group(&channel, &groupx, &rolegroup);
    switch (rolegroup)
    {
    case 0:
	channel->_Chan_UInts = groupx;
	break;
    case 1:
	channel->_Chan_PInts = groupx;
	break;
    case 2:
	channel->_Chan_UPInts = groupx;
	break;
    }
    while (symbol == SYM_BY)
    {
	grouppointer tmp;
	interaction_group(&channel, &groupx, &rolegroup);
	tmp = groupx;
	while (tmp->next)
	    tmp = tmp->next;
	switch (rolegroup)
	{
	case 0:
	    tmp->next = channel->_Chan_UInts;
	    channel->_Chan_UInts = groupx;
	    break;
	case 1:
	    tmp->next = channel->_Chan_PInts;
	    channel->_Chan_PInts = groupx;
	    break;
	case 2:
	    tmp->next = channel->_Chan_UPInts;
	    channel->_Chan_UPInts = groupx;
	    break;
	}
    }
    exitblock();
    chanQpri = 0;
}

/***********************************************************************/
/* InteractionDefinition ::- Identifier [ "(" ParameterList ")" ] ";". */
/***********************************************************************/

static pointer NEAR 
interaction_definition(short Qtyp)
{
    short length;
    pointer lastparam, interactionx;
    interactionx = define_nextident(INTERACTION_);
    interactionx->_Intr = (interactionpointer) EC_malloc(sizeof(struct interactionrecord));
    if (symbol == SYM_LEFTPARENTHESIS)
    {
	nextsymbol();
	new_block(TRUE);
	length = parameter_list(INTERACTIONARG, &lastparam);
	parameter_addressing(length, lastparam, 0);
	exitblock();
	expect(SYM_RIGHTPARENTHESIS);
    } else
    {
	lastparam = NULL;
	length = 0;
    }
    interactionx->_Intr_LastArg = lastparam;
    interactionx->_Intr_ArgLen = length;
    interactionx->_Intr_HasWhen = FALSE;
    interactionx->_Intr_HasOutput = FALSE;
    interactionx->_Intr_Priority = chanQpri;
    if (Qtyp == PRIORITY_Q)
	chanQpri++;
    expect(SYM_SEMICOLON);
    return interactionx;
}

/************************************************************************/
/* InteractionGroup ::- "by" RoleIdentifier [ "," RoleIdentifier ] ":"	*/
/*		       [ InteractionDefinition ]^1.			*/
/************************************************************************/

static void NEAR 
interaction_group(pointer * channel, grouppointer * group, short *rolegroup)
{
    pointer role, role2;
    grouppointer group2;
    expect(SYM_BY);
    role = role_ident();
    *rolegroup = (role == (*channel)->_Chan_User) ? 0 : 1;
    if (symbol == SYM_COMMA)
    {
	nextsymbol();
	role2 = role_ident();
	if (role == role2)
	    compiler_error(212, ERR_ROLES);
	*rolegroup = 2;
    }
    *group = (grouppointer) EC_malloc(sizeof(struct grouprecord));
    (*group)->next = NULL;
    expect(SYM_COLON);
    (*group)->interaction = interaction_definition((*channel)->_Chan_QType);
    while (symbol == SYM_IDENTIFIER)
    {
	group2 = (grouppointer) EC_malloc(sizeof(struct grouprecord));
	group2->interaction = interaction_definition((*channel)->_Chan_QType);
	group2->next = *group;
	*group = group2;
    }
}

/************************************************************************/
/* InteractionPointType ::- ChannelIdentifier "(" RoleIdentifier ")"	*/
/*			   [ QueueDiscipline ].				*/
/************************************************************************/

static pointer NEAR 
IPtype(void)
{
    pointer type, channel, role = 0;
    short ident, discipline;
    type = define(NOIDENTIFIER, IPTYPE);
    type->_IPTyp = (IPpointer) EC_malloc(sizeof(struct IPrecord));
    channel = channel_ident();
    expect(SYM_LEFTPARENTHESIS);
    ident = expect_ident();
    if (channel->_Chan_User->ident == ident)
	role = channel->_Chan_User;
    else if (channel->_Chan_Prov->ident == ident)
	role = channel->_Chan_Prov;
    else
	undef_ident_err(213, ID_NameStore + ID_MapTable[ident]);
    expect(SYM_RIGHTPARENTHESIS);
    if (symbol == SYM_COMMON || symbol == SYM_INDIVIDUAL)
	discipline = queue_discipline();
    else
    {
	if (default_discipline == -1)
	    compiler_error(214, ERR_NODEFAULT);
	discipline = default_discipline;
    }
    type->_IPTyp_Chan = channel;
    type->_IPTyp_Role = role;
    type->_IPTyp_Disc = discipline;
    type->_IPTyp_Delay = IPQdelay;
    return type;
}

/****************************************************************/
/* InteractionReference ::- InteractionPointReference		*/
/*				"." InteractionIdentifier.	*/
/****************************************************************/

pointer 
interaction_reference(void)
{
    pointer interactionx = 0, ipx, channel, role;
    short ident;
    grouppointer nextinteraction;
    BOOLEAN found;
    ipx = IP_reference();
    expect(SYM_PERIOD);
    ident = expect_ident();
    channel = ipx->_IPTyp_Chan;
    role = ipx->_IPTyp_Role;
    if (role == channel->_Chan_User || role == channel->_Chan_Prov)
    {
	if (role == channel->_Chan_User)
	    nextinteraction = channel->_Chan_UInts;
	else
	    nextinteraction = channel->_Chan_PInts;
	found = FALSE;
	while (!found && nextinteraction != NULL)
	{
	    if (ident == nextinteraction->interaction->ident)
	    {
		interactionx = nextinteraction->interaction;
		found = TRUE;
	    } else
		nextinteraction = nextinteraction->next;
	}
	nextinteraction = channel->_Chan_UPInts;
	while (!found && nextinteraction != NULL)
	{
	    if (ident == nextinteraction->interaction->ident)
	    {
		interactionx = nextinteraction->interaction;
		found = TRUE;
	    } else
		nextinteraction = nextinteraction->next;
	}
	if (!found)
	    undef_ident_err(215, ID_NameStore + ID_MapTable[ident]);
    } else
    {
	interactionx = define(ident, INTERACTION_);
	interactionx->_Intr = (interactionpointer) EC_malloc(sizeof(struct interactionrecord));
	interactionx->_Intr_LastArg = NULL;
	interactionx->_Intr_ArgLen = 0;
    }
    interactionx->_Intr_IPType = ipx;
    return interactionx;
}

/**********************************************/
/* IpIndex ::- Constant | VariableIdentifier. */
/* IpIndexList ::- IpIndex [ "," IpIndex ]^0. */
/* IpIndexedSelector ::- "[" IpIndexList "]" .*/
/**********************************************/

static pointer NEAR 
ipindex(void)
{
    short tmp;
    valuepointer value;
    pointer object, ipindextype;
    if (symbol == SYM_IDENTIFIER)
    {
	object = find(argument);
	switch (object->kind)
	{
	case VARIABLE:
	case VALUEPARAMETER:
	case VARPARAMETER:
	case INTERACTIONARG:
	    nextsymbol();
	    ipindextype = object->type;
	    emit(OP_VARIABLE, object->_Var_Level, object->place);
	    emit(OP_VALUE, tmp = typelength(object->type));
	    push(tmp - 1);
	    break;
	default:
	    ipindextype = constant(&value);
	    emit(OP_CONSTANT, value->_Val_Int);
	    break;
	}
    } else
    {
	ipindextype = constant(&value);
	emit(OP_CONSTANT, value->_Val_Int);
    }
    return ipindextype;
}

static void NEAR 
IP_indexed_selector(pointer * type)
{
    pointer ipindextype;
    expect(SYM_LEFTBRACKET);
    ipindextype = ipindex();
    if ((*type)->kind == ARRAYTYPE)
    {
	checktypes(ipindextype, (*type)->_Aray_RangType);
	emit(OP_INDEX, (*type)->_Aray_LowerVal, (*type)->_Aray_UpperVal, typelength((*type)->type));
	*type = (*type)->type;
	while (symbol == SYM_COMMA)
	{
	    nextsymbol();
	    ipindextype = ipindex();
	    if ((*type)->kind == ARRAYTYPE)
	    {
		checktypes(ipindextype, (*type)->_Aray_RangType);
		emit(OP_INDEX, (*type)->_Aray_LowerVal, (*type)->_Aray_UpperVal, typelength((*type)->type));
		*type = (*type)->type;
	    } else
		compiler_error(216, ERR_TYPE);
	}
    } else
	compiler_error(217, ERR_TYPE);
    expect(SYM_RIGHTBRACKET);
}

/********************************************************/
/* ExternalIp ::- InteractionPointReference.		*/
/* ChildExternalIp ::- ModuleVariable "." ExternalIp.	*/
/********************************************************/

pointer 
childexternalip(void)
{
    pointer modvar, type, ipx;
    short ident;
    BOOLEAN found;
    modvar = module_variable(FALSE);
    expect(SYM_PERIOD);
    if (symbol == SYM_IDENTIFIER)
    {
	ident = expect_ident();
	ipx = modvar->_Mod_LastEntry;
	found = FALSE;
	while (ipx && !found)
	{
	    if (ipx->ident == ident)
		found = TRUE;
	    else
		ipx = ipx->previous;
	}
	if (found)
	{
	    emit(OP_CONSTANT, ident);
	    emit(OP_CONSTANT, ipx->place);
	    type = ipx->type;
	    if (symbol == SYM_LEFTBRACKET)
		indexed_selector(&type);
	} else
	    undef_ident_err(201, ID_NameStore + ID_MapTable[ident]);
    } else
	compiler_error(202, ERR_NOIDENT);
    return type;
}

/************************************************/
/* InternalIp ::- InteractionPointReference.	*/
/* ConnectIp ::- ChildExternalIp | InternalIp.	*/
/************************************************/

pointer 
connectip(BOOLEAN * internal)
{
    pointer object, ipx = 0;
    if (symbol == SYM_IDENTIFIER)
    {
	object = find(argument);
	if (object->kind == MODVAR)
	{
	    *internal = FALSE;
	    ipx = childexternalip();
	} else
	{
	    *internal = TRUE;
	    ipx = IP_reference();
	}
    } else
	compiler_error(210, ERR_NOIDENT);
    return ipx;
}

/************************************************************************/
/* IpTypeDenoter ::- InteractionPointType | StructuredIpType.		*/
/* IpGroup ::- Identifier GroupTail.					*/
/* IpGroupTail ::- "," IpGroup | ":" IpTypeDenoter.			*/
/* InteractionPointDeclaration ::- IdentifierList ":" IpTypeDenoter.	*/
/* InteractionPointDeclarationPart ::- "ip"				*/
/*		[InteractionPointDeclaration ";"]^1.			*/
/* StructuredIpType ::- "array" "[" IndexTypeList "]"			*/
/*				"of" InteractionPointType.		*/
/************************************************************************/

pointer 
IP_reference(void)
{
    pointer type, ipx = IP_ident();
    emit(OP_CONSTANT, ipx->ident);
    emit(OP_CONSTANT, ipx->place);
    type = ipx->type;
    if (symbol == SYM_LEFTBRACKET)
	indexed_selector(&type);
    return type;
}

static void NEAR 
IP_group(pointer * type, short *number)
{
    pointer ipx = define_nextident(IP_);
    if (symbol == SYM_COMMA)
    {
	nextsymbol();
	IP_group(type, number);
	*number += 1;
    } else
    {
	expect(SYM_COLON);
	if (symbol == SYM_ARRAY)
	{
	    pointer tmp = array_specification(type);
	    tmp->type = IPtype();
	} else
	    *type = IPtype();
	*number = 1;
    }
    ipx->type = *type;
}


static short NEAR 
IP_declaration(void)
{
    pointer type;
    short number;
    IP_group(&type, &number);
    IPQdelay = 0;
    number *= typelength(type);
    return number;
}

void 
IP_declaration_part(short IPcnt, short firstindex)
{
    short cnt;
    expect(SYM_IP);
    cnt = IP_declaration();
    expect(SYM_SEMICOLON);
    while (symbol == SYM_IDENTIFIER)
    {
	cnt += IP_declaration();
	expect(SYM_SEMICOLON);
    }
    emit(OP_DEFARG, IPcnt, cnt);
    table_addressing(cnt, firstindex + cnt, IP_);
}

/****************************************************************/
/* QueueDiscipline ::- "common" "queue" | "individual" "queue". */
/****************************************************************/

static short NEAR 
queue_discipline(void)
{
    short rtn = 0;
    if (symbol == SYM_COMMON)
	nextsymbol();
    else
    {
	rtn = 1;
	expect(SYM_INDIVIDUAL);
    }
    expect(SYM_QUEUE);
    return rtn;
}

/************************************************************************/
/* StateIdentifier ::- Identifier. 					*/
/* StateSetConstant ::- "[" StateIdentifier [ "," StateIdentifier ]^0 "]". */
/* StateSetDefinition ::- Identifier "=" StateSetConstant. 		*/
/* StateSetDefinitionPart ::- "stateset" [ StateSetDefinition ";" ]^1.	*/
/************************************************************************/

static pointer NEAR 
state_ident(void)
{
    pointer object = gen_ident(CONSTANTX);
    if (object->type->kind != STATEX)
	compiler_error(220, ERR_TYPE);
    return object;
}

static pointer NEAR 
statesetconstant(void)
{
    pointer object, stset_const;
    short i, value;
    expect(SYM_LEFTBRACKET);
    object = state_ident();
    stset_const = define(NOIDENTIFIER, CONSTANTX);
    stset_const->type = object->type;
    stset_const->_Const_Val = (valuepointer) EC_malloc(sizeof(struct valuerecord));
    stset_const->_Const_Set = (setconstantpointer) EC_malloc(sizeof(setconstanttype));
    for (i = 0; i < SETSIZE; i++)
	(*(stset_const->_Const_Set))[i] = 0;

    value = object->_Const_Int;
    add2set((*(stset_const->_Const_Set)), value);
    while (symbol == SYM_COMMA)
    {
	nextsymbol();
	value = state_ident()->_Const_Int;
	add2set((*(stset_const->_Const_Set)), value);
    }
    expect(SYM_RIGHTBRACKET);
    return stset_const;
}

static void NEAR 
stateset_definition(void)
{
    pointer object = define_nextident(STATESETX);
    expect(SYM_EQUAL);
    object->_StSet_Val = statesetconstant();
}

void 
stateset_definition_part(void)
{
    expect(SYM_STATESET);
    stateset_definition();
    expect(SYM_SEMICOLON);
    while (symbol == SYM_IDENTIFIER)
    {
	stateset_definition();
	expect(SYM_SEMICOLON);
    }
}
