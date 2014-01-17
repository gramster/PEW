/* pasparse.c - pascal construct parser */

#include "symbol.h"
#include "compiler.h"

/* Prototypes of local functions */

static void NEAR blockx(short beginlabel, short varlabel, short templabel);
static void NEAR formal_parameter_section(pointer * lastparam, short *length);
static void NEAR func_parameter_specification(void);
static void NEAR label_declaration_part(void);

static void NEAR new_enumerated_type(pointer type);
static void NEAR new_subrange_type(pointer type);
static pointer NEAR new_ordinal_type(void);
static pointer NEAR new_arraytype(void);
static void NEAR field_list(pointer * lastfield, short startoffset, short *length);
static pointer NEAR new_type(void);

static void NEAR proc_parameter_specification(void);

static void NEAR type_definition(void);
static pointer NEAR type_denoter(BOOLEAN param);
static pointer variable_group(enum class kind, short *number, pointer * lastvar, BOOLEAN param);

static short NEAR value_parameter_specification(enum class kind, pointer * lastparam);
static void NEAR variable_parameter_specification(pointer * lastparam, short *number);

/***************************************
	VARIABLE ADDRESSING
*****************************************/

/* I think these three could be combined fairly easily, using a switch
on the kind, and passing initial and final displacement valiues thru */

static void NEAR 
field_addressing(short startoffset, short recordlength, pointer lastfield)
{
    short displ = recordlength;
    while (displ > startoffset)
    {
	displ -= typelength(lastfield->type);
	lastfield->place = displ;
	do
	{
	    lastfield = lastfield->previous;
	} while (lastfield && lastfield->ident == NOIDENTIFIER);
    }
}

static void NEAR 
variable_addressing(short varlength, pointer lastvar)
{
    short displ = 3 + varlength;
    while (displ > 3)
    {
	displ -= typelength(lastvar->type);
	lastvar->_Var_Level = blocklevel;
	lastvar->place = displ;
	do
	{
	    lastvar = lastvar->previous;
	} while (lastvar && lastvar->ident == NOIDENTIFIER);
    }
}


void 
parameter_addressing(short paramlength, pointer lastparam, short rtn_len)
{
    short displ = -rtn_len;
    while (displ > (-paramlength - rtn_len))
    {
	if (lastparam->kind == VARPARAMETER)
	    displ--;
	else
	    displ -= typelength(lastparam->type);
	lastparam->_Var_Level = blocklevel;
	lastparam->place = displ;
	do
	{
	    lastparam = lastparam->previous;
	} while (lastparam && lastparam->ident == NOIDENTIFIER);
    }
}


/*************************************/
/* Identifier lists for declarations */
/*************************************/

void 
    ident_list(pointer * last, short *number, enum class kind)
    {
	pointer tmp = define_nextident(kind);
	   *number = 1;
	while (symbol == SYM_COMMA)
	{
	    nextsymbol();
	    tmp = define_nextident(kind);
	    *number += 1;
	}
	   *last = tmp;
	expect(SYM_COLON);
    }

void 
assign_type(pointer last, short number, pointer type)
{
    while (number-- > 0)
    {
	last->type = type;
	last = last->previous;
    }
}

/************************************************************************/
/* ArraySpecification ::- "array" "[" IndexList "]" "of".		*/
/************************************************************************/

pointer 
array_specification(pointer * type)
{
    pointer temptype1, temptype2;
    temptype1 = *type = define(NOIDENTIFIER, ARRAYTYPE);
    expect(SYM_ARRAY);
    expect(SYM_LEFTBRACKET);
    (*type)->_Aray_IxType = ordinal_type();
    while (symbol == SYM_COMMA)
    {
	nextsymbol();
	temptype2 = define(NOIDENTIFIER, ARRAYTYPE);
	temptype1->type = temptype2;
	temptype2->_Aray_IxType = ordinal_type();
	temptype1 = temptype2;
    }
    expect(SYM_RIGHTBRACKET);
    expect(SYM_OF);
    return temptype1;
}

/************************************************************************/
/* ActualParameterList ::- [ ActualParameterList "," ] ActualParameter.	*/
/* ActualParameter     ::- Expression | VariableAccess.			*/
/************************************************************************/

void 
actual_param_list(pointer lastparam, short *length)
{
    pointer type;
    short more;

    if (lastparam->previous != NULL)
    {
	actual_param_list(lastparam->previous, &more);
	expect(SYM_COMMA);
    } else
	more = 0;
    if (lastparam->kind != VARPARAMETER)
    {
	type = expression();
	*length = typelength(type) + more;
    } else
    {
	type = variable_access(TRUE, FALSE);
	*length = 1 + more;
    }
}

/****************************************************************/
/* StatementPart ::- CompoundStatement.				*/
/* Block ::- LabelDeclarationPart ConstantDefinitionPart	*/
/*		TypeDefinitionPart VariableDeclarationPart	*/
/*		ProcAndFuncDeclarationPart StatementPart.	*/
/****************************************************************/

static void NEAR 
blockx(short beginlabel, short varlabel, short templabel)
{
    short length = 0;
    if (symbol == SYM_LABEL)
	label_declaration_part();
    if (symbol == SYM_CONST)
	constant_definition_part();
    if (symbol == SYM_TYPE)
	type_definition_part();
    if (symbol == SYM_VAR)
	length = variable_declaration_part(0);
    if (symbol == SYM_PURE || symbol == SYM_PROCEDURE || symbol == SYM_FUNCTION)
	proc_and_func_decl_part();
    emit(OP_DEFADDR, beginlabel);
    block[blocklevel].varlength = length;
    compound_statement();
    emit(OP_DEFARG, varlabel, block[blocklevel].varlength);
    emit(OP_DEFARG, templabel, block[blocklevel].maxtemp);
    if (block[blocklevel].label.value != 0)
	if (!block[blocklevel].label.resolved)
	    compiler_error(102, ERR_UNRESOLVED);
}

/************************************************************************/
/* UnsignedNumber ::- Integer.						*/
/* ConstantIdentifier ::- Identifier.					*/
/* SignOperator ::- "+" | "-". 						*/
/* CharacterString ::- "'" StringElement [ StringElement ]^0 "'".	*/
/* Constant ::- [ SignOperator ]					*/
/*			( UnsignedNumber | ConstantIdentifier ) 	*/
/*	    	| CharacterString.			       		*/
/************************************************************************/

pointer 
constant(valuepointer * value)
{
    register pointer rtn = 0;
    pointer object;
    short minus = 1;
    switch (symbol)
    {
    case SYM_MINUS:
	minus = -1;
    case SYM_PLUS:
	nextsymbol();
    default:
	break;
    }
    if (symbol == SYM_INTEGER)
    {
	*value = (valuepointer) EC_malloc(sizeof(struct valuerecord));
	(*value)->_Val_Int = argument * minus;
	rtn = typeinteger;
	nextsymbol();
    } else if (symbol == SYM_IDENTIFIER)
    {
	object = find(argument);
	if (argument == _SIZEOF)
	{
	    nextsymbol();
	    expect(SYM_LEFTPARENTHESIS);
	    object = find(argument);
	    *value = (valuepointer) EC_malloc(sizeof(struct valuerecord));
	    rtn = typeinteger;
	    if (object->kind == VARIABLE)
		object = object->type;
	    (*value)->_Val_Int = minus * typelength(object);
	    nextsymbol();
	    expect(SYM_RIGHTPARENTHESIS);
	} else if (object->kind == CONSTANTX)
	{
	    *value = (valuepointer) EC_malloc(sizeof(struct valuerecord));
	    rtn = object->type;
	    if (isin(object->type, INTEGERS))
		(*value)->_Val_Int = object->_Const_Int * minus;
	    else
	    {
		*value = object->_Const_Val;
		if (minus == -1)
		    compiler_error(103, ERR_SIGNED);
	    }
	    nextsymbol();
	} else
	    compiler_error(104, ERR_TYPE);
    } else if (symbol == SYM_GRAPHIC)
    {
	pointer temptype1;
	*value = (valuepointer) EC_malloc(sizeof(struct valuerecord));
	if (strlen(ec_buffer) > 1)
	{
	    (*value)->_Val_Str = (stringpointer) EC_malloc(sizeof(struct stringrecord));
	    strcpy((*value)->_Val_String, ec_buffer);
	    rtn = define(NOIDENTIFIER, ARRAYTYPE);
	    temptype1 = define(NOIDENTIFIER, SUBRANGETYPE);
	    temptype1->_Sub = (subrgpointer) EC_malloc(sizeof(struct subrgrecord));
	    temptype1->_Sub_Lower = (valuepointer) EC_malloc(sizeof(struct valuerecord));
	    temptype1->_Sub_LowerVal = 1;
	    temptype1->_Sub_Upper = (valuepointer) EC_malloc(sizeof(struct valuerecord));
	    temptype1->_Sub_UpperVal = (int)strlen(ec_buffer);
	    temptype1->type = typeinteger;
	    rtn->_Aray_IxType = temptype1;
	    rtn->type = typechar;
	    (*value)->_Val_Str->definedtype = rtn;
	} else
	{
	    (*value)->_Val_Int = (int)(ec_buffer[0]);
	    rtn = typechar;
	}
	nextsymbol();
    } else
	compiler_error(105, ERR_BADCONSTANT);
    return rtn;
}

/************************************************************************/
/* ConstantDefinition ::- ConstantIdentifier "="			*/
/*			 ( Constant | "any" TypeIdentifier.		*/
/* ConstantDefinitionPart ::- "const" [ ConstantDefinition ";" ]^1 	*/
/************************************************************************/

void 
constant_definition_part(void)
{
    valuepointer value;
    pointer constx, type;
    expect(SYM_CONST);
    do
    {
	constx = define_nextident(CONSTANTX);
	expect(SYM_EQUAL);
	if (symbol == SYM_ANY)
	{
	    expect(SYM_ANY);
	    type = type_denoter(TRUE);
	    switch (type->kind)
	    {
	    case SUBRANGETYPE:
		value = type->_Sub_Lower;
		if (value->_Val_Int < 0)
		{
		    value = type->_Sub_Upper;
		    if (value->_Val_Int > 0)
		    {
			value = (valuepointer) EC_malloc(sizeof(struct valuerecord));
			value->_Val_Int = 0;
		    }
		}
		break;
	    case ENUMERATEDTYPE:
	    case STANDARDTYPE:
		value = (valuepointer) EC_malloc(sizeof(struct valuerecord));
		value->_Val_Int = 0;
		break;
	    default:
		compiler_error(210, ERR_ANYCONST);
	    }
	} else
	    type = constant(&value);
	constx->_Const_Val = value;
	constx->type = type;
	expect(SYM_SEMICOLON);
    } while (symbol == SYM_IDENTIFIER);
}

/********************************************************/
/* StateDefinitionPart ::- "state" IdentifierList ";". 	*/
/* Declarations	::- ConstantDefinitionPart		*/
/*		  | TypeDefinitionPart			*/
/*		  | ChannelDefinition 			*/
/*		  | ModuleHeaderDefinition		*/
/*		  | ModuleBodyDefinition		*/
/*		  | InteractionPointDeclarationPart	*/
/*		  | ModuleVariableDeclarationPart	*/
/*		  | VariableDeclarationPart		*/
/*		  | StateDefinitionPart			*/
/*		  | StateSetDefinitionPart		*/
/*		  | ProcAndFuncDeclarationPart.		*/
/* DeclarationPart ::- [ Declarations ]^0.		*/
/********************************************************/

short 
declaration_part(short modvarcnt, short IPcnt, short extIPcnt, short *numstates)
{				/* bug in original - does not return value */
    short length = 0;
    *numstates = 0;
    while (TRUE)
    {
	switch (symbol)
	{
	case SYM_CONST:
	    constant_definition_part();
	    break;
	case SYM_TYPE:
	    type_definition_part();
	    break;
	case SYM_CHANNEL:
	    channel_definition();
	    break;
	case SYM_MODULE:
	    moduleheader_definition();
	    break;
	case SYM_BODY:
	    modulebody_definition();
	    break;
	case SYM_IP:
	    IP_declaration_part(IPcnt, extIPcnt);
	    break;
	case SYM_MODVAR:
	    modvar_declaration_part(modvarcnt);
	    break;
	case SYM_VAR:
	    length += variable_declaration_part(length);
	    break;
	case SYM_STATE:
	    if (*numstates)
		compiler_error(106, ERR_STATES);
	    else
	    {
		pointer tempconstant1, tempconstant2, state_typ;
		expect(SYM_STATE);
		tempconstant1 = define_nextident(CONSTANTX);
		tempconstant1->type = state_typ = define(NOIDENTIFIER, STATEX);
		tempconstant1->_Const_Val = (valuepointer) EC_malloc(sizeof(struct valuerecord));
		tempconstant1->_Const_Int = (*numstates)++;
		while (symbol == SYM_COMMA)
		{
		    nextsymbol();
		    tempconstant2 = define_nextident(CONSTANTX);
		    tempconstant2->type = state_typ;
		    tempconstant2->_Const_Val = (valuepointer) EC_malloc(sizeof(struct valuerecord));
		    tempconstant2->_Const_Int = (*numstates)++;
		    tempconstant1->_Next_Const = tempconstant2;
		    tempconstant1 = tempconstant2;
		}
		tempconstant1->_Next_Const = NULL;
		expect(SYM_SEMICOLON);
	    }
	    break;
	case SYM_STATESET:
	    stateset_definition_part();
	    break;
	case SYM_PROCEDURE:
	case SYM_FUNCTION:
	    proc_and_func_decl_part();
	    break;
	default:
	    return length;
	}
    }
}

/************************************************/
/* FieldIdentifier ::- Identifier. 		*/
/* FieldSpecifier ::- FieldIdentifier. 		*/
/* FieldSelector ::- "." FieldSpecifier. 	*/
/************************************************/

static void NEAR 
field_selector(pointer * type)
{
    BOOLEAN found;
    pointer field;
    expect(SYM_PERIOD);
    if (symbol == SYM_IDENTIFIER)
    {
	if ((*type)->kind == RECORDTYPE)
	{
	    found = FALSE;
	    field = (*type)->_Rec_LastFld;
	    while (!found && field != NULL)
	    {
		if (field->ident != argument)
		    field = field->previous;
		else
		    found = TRUE;
	    }
	    if (found)
	    {
		*type = field->type;
		emit(OP_FIELD, field->place);
	    } else
		undef_ident_err(107, ID_NameStore + ID_MapTable[argument]);
	} else
	    compiler_error(108, ERR_TYPE);
	expect(SYM_IDENTIFIER);
    } else
	compiler_error(109, ERR_NOIDENT);
}

/****************************************************************/
/* FormalParameterList ::- FormalParameterSection		*/
/*			  [ ";" FormalParameterSection ]^1.	*/
/****************************************************************/

static void NEAR 
formal_parameter_list(pointer * lastparam, short *length)
{
    short more;
    formal_parameter_section(lastparam, length);
    while (symbol == SYM_SEMICOLON)
    {
	nextsymbol();
	formal_parameter_section(lastparam, &more);
	*length += more;
    }
}

/************************************************************************/
/* FormalParameterSection ::- ValueParameterSpecification.		*/
/*			  | VariableParameterSpecification. 		*/
/*			  | [ "pure" ] ProceduralParameterSpecification.*/
/*			  | [ "pure" ] FunctionalParameterSpecification.*/
/************************************************************************/

static void NEAR 
formal_parameter_section(pointer * lastparam, short *length)
{
    if (symbol == SYM_IDENTIFIER)
	*length = value_parameter_specification(VALUEPARAMETER, lastparam);
    else if (symbol == SYM_VAR)
	variable_parameter_specification(lastparam, length);
    else
    {
	if (symbol == SYM_PURE)
	    nextsymbol();
	if (symbol == SYM_FUNCTION)
	    func_parameter_specification();
	else if (symbol == SYM_PROCEDURE)
	    proc_parameter_specification();
	else
	    compiler_error(110, ERR_BADFORMLPARM);
    }
}


/************************************************************************/
/* ResultType ::- TypeIdentifier.					*/
/* FunctionHeading ::- "function" Identifier				*/
/*		      [ "(" FormalParameterList ")" ] ":" ResultType.	*/
/* FunctionalParameterSpecification ::- FunctionHeading.		*/
/************************************************************************/

static void NEAR 
func_parameter_specification(void)
{
    compiler_error(200, ERR_FUNCPARAM);
}

/********************************************************/
/* BodyIdentifier ::- Identifier.			*/
/* HeaderIdentifier ::- Identifier.			*/
/* ModuleVariableIdentifier ::- Identifier.		*/
/* ConstantIdentifier ::- Identifier.			*/
/* ChannelIdentifier ::- Identifier.			*/
/* RoleIdentifier ::- Identifier.			*/
/* StateSetIdentifier ::- Identifier.			*/
/* InteractionPointIdentifier ::- Identifier.		*/
/* InteractionIdentifier ::- Identifier.		*/
/* InteractionArgumentIdentifier ::- Identifier.	*/
/* VariableIdentifier ::- Identifier.			*/
/********************************************************/

pointer 
    gen_ident(enum class kind)
    {
	pointer object = 0;
	if  (symbol == SYM_IDENTIFIER)
	{
	    object = find(argument);
	    if (object->kind != kind)
		compiler_error(111, ERR_TYPE);
	}
	    expect(SYM_IDENTIFIER);
	return object;
    }

/****************************************************************/
/* IndexExpression ::- Expression. }				*/
/* IndexExpressionList ::- IndexExpression [ ";"		*/
/*				IndexExpression ]^0. }		*/
/* IndexedSelector ::- "[" IndexExpressionList "]" .}		*/
/****************************************************************/


void 
indexed_selector(pointer * type)
{
    pointer expr;
    expect(SYM_LEFTBRACKET);
    expr = expression();
    if ((*type)->kind == ARRAYTYPE)
    {
	if (expr->kind == SUBRANGETYPE)
	{
	    checktypes(expr, (*type)->_Aray_IxType);
	} else
	{
	    checktypes(expr, (*type)->_Aray_RangType);
	}
	emit(OP_INDEX, (*type)->_Aray_LowerVal, (*type)->_Aray_UpperVal, typelength((*type)->type));
	*type = (*type)->type;
	while (symbol == SYM_COMMA)
	{
	    nextsymbol();
	    expr = expression();
	    if ((*type)->kind == ARRAYTYPE)
	    {
		if (expr->kind == SUBRANGETYPE)
		{
		    checktypes(expr, (*type)->_Aray_IxType);
		} else
		{
		    checktypes(expr, (*type)->_Aray_RangType);
		}
		emit(OP_INDEX, (*type)->_Aray_LowerVal, (*type)->_Aray_UpperVal, typelength((*type)->type));
		*type = (*type)->type;
	    } else
		compiler_error(112, ERR_TYPE);
	}
    } else
	compiler_error(113, ERR_TYPE);
    expect(SYM_RIGHTBRACKET);
}

/***********************************************/
/* LabelDeclarationPart ::- [ "label" Label ]. */
/***********************************************/

static void NEAR 
label_declaration_part(void)
{
    nextsymbol();
    block[blocklevel].label.value = argument;
    block[blocklevel].label.number = new_label();
    expect(SYM_INTEGER);
}

/************************************************************************/
/* IdentifierList ::- Identifier [ ";" Identifier ]^0.			*/
/* NewEnumeratedType ::- "(" IdentifierList ")".			*/
/*									*/
/* NewSubrangeType ::- Constant ".." Constant. 				*/
/*									*/
/* NewOrdinalType ::- NewEnumeratedType | NewSubrangeType. 		*/
/*									*/
/* TagField ::- Identifier.						*/
/* TagType ::- OrdinalTypeIdentifier.					*/
/* VariantSelector ::- [ TagField ":" ] TagType.			*/
/* CaseConstant ::- Constant.						*/
/* CaseConstantList ::- CaseConstant [ "," CaseConstant ]^0.		*/
/* RecordSection ::- IdentifierList ":" TypeDenoter. 			*/
/* Variant ::- CaseConstantList ":" "(" FieldList ")".			*/
/* VariantPart ::- "case" VariantSelector "of" Variant [ ";" Variant ]^0. */
/* FixedPart ::- RecordSection [ ";" RecordSection ]^0.			*/
/* FieldList ::- [ ( FixedPart [ ";" VariantPart ] | VariantPart )	*/
/*			[ ";" ] ].					*/
/* NewRecordType ::- "record" FieldList "end". 				*/
/*									*/
/* IndexType ::- OrdinalType.						*/
/* IndexTypeList ::- IndexType [ "," IndexType ]^0.			*/
/* ComponentType ::- TypeDenoter.					*/
/* NewArrayType ::- "array" "[" IndexTypeList "]" "of" ComponentType.	*/
/*									*/
/* OrdinalTypeIdentifier ::- TypeIdentifier.				*/
/* OrdinalType ::- NewOrdinalType | OrdinalTypeIdentifier.		*/
/* BaseType ::- OrdinalType.						*/
/* NewSetType ::- "set" "of" BaseType.					*/
/*									*/
/* DomainType := TypeIdentifier.					*/
/* NewPointerType ::- "^" DomainType.					*/
/*									*/
/* UnpackedStructuredType ::- NewArrayType | NewRecordType | NewSetType. */
/* NewStructuredType ::- [ "packed" ] UnpackedStructuredType. 		*/
/* NewType ::- NewOrdinalType | NewStructuredType | NewPointerType. 	*/
/************************************************************************/

static void NEAR 
new_enumerated_type(pointer type)
{
    pointer tempconstant1, tempconstant2;
    short value = 0;
    expect(SYM_LEFTPARENTHESIS);
    tempconstant1 = define_nextident(CONSTANTX);
    tempconstant1->type = type;
    tempconstant1->_Const_Val = (valuepointer) EC_malloc(sizeof(struct valuerecord));
    tempconstant1->_Const_Int = 0;
    type->kind = ENUMERATEDTYPE;
    type->_Enum = (enumpointer) EC_malloc(sizeof(struct enumrecord));
    type->_Enum_First = tempconstant1;
    while (symbol == SYM_COMMA)
    {
	nextsymbol();
	tempconstant2 = define_nextident(CONSTANTX);
	tempconstant2->type = type;
	tempconstant2->_Const_Val = (valuepointer) EC_malloc(sizeof(struct valuerecord));
	tempconstant2->_Const_Int = ++value;
	tempconstant1->_Next_Const = tempconstant2;
	tempconstant1 = tempconstant2;
    }
    type->_Enum_Last = tempconstant1;
    tempconstant1->_Next_Const = NULL;
    expect(SYM_RIGHTPARENTHESIS);
}

/******************************************/

static void NEAR 
new_subrange_type(pointer type)
{
    pointer temptype1, temptype2;
    valuepointer tempvalue1, tempvalue2;
    temptype1 = constant(&tempvalue1);
    type->kind = SUBRANGETYPE;
    type->_Sub = (subrgpointer) EC_malloc(sizeof(struct subrgrecord));
    if (isin(temptype1, ORDINALS))
	type->type = temptype1;
    else
	compiler_error(114, ERR_ORDINAL);
    expect(SYM_DOUBLEDOT);
    temptype2 = constant(&tempvalue2);
    checktypes(temptype1, temptype2);
    if ((temptype1 == temptype2) && isin(temptype1, ORDINALS))
    {
	if (tempvalue1->_Val_Int > tempvalue2->_Val_Int)
	    compiler_error(115, ERR_RANGE);
    } else
	compiler_error(116, ERR_ORDINAL);
    type->_Sub_Lower = tempvalue1;
    type->_Sub_Upper = tempvalue2;
}

/***************************************************/

static pointer NEAR 
new_ordinal_type(void)
{
    pointer rtn = define(NOIDENTIFIER, UNDEFINED);
    switch (symbol)
    {
    case SYM_LEFTPARENTHESIS:
	new_enumerated_type(rtn);
	break;
    case SYM_IDENTIFIER:
    case SYM_INTEGER:
    case SYM_GRAPHIC:
    case SYM_MINUS:
    case SYM_PLUS:
	new_subrange_type(rtn);
	break;
    default:
	break;
    }
    return rtn;
}

/******************************************/

static pointer NEAR 
new_arraytype(void)
{
    pointer tmp, type;
    tmp = array_specification(&type);
    tmp->type = type_denoter(FALSE);
    return type;
}

/********************************************************/

void 
case_constant_list(pointer caseindextype, short caselabel[], short statementlabel)
{
    valuepointer casevalptr;
    BOOLEAN firstcase = TRUE;
    short caseval;

    /*
     * if (caseindextype) ; Check that bounds of type are within 0..MAX-1
     * range
     */
    do
    {
	if (!firstcase)
	    nextsymbol();	/* skip COMMA */
	else
	    firstcase = FALSE;
	if (constant(&casevalptr) != caseindextype)
	    compiler_error(120, ERR_SELECTORTYPE);
	caseval = casevalptr->_Val_Int;
	if (caseval < 0 || caseval >= MAXCASEINDICES)
	    compiler_error(121, ERR_INDICES);
	if (caselabel[caseval])
	    compiler_error(122, ERR_REPETITION);
	caselabel[caseval] = statementlabel;
    } while (symbol == SYM_COMMA);
}

static void NEAR 
fixed_part(pointer * lastfield, short startoffset, short *length)
{
    short number, num;
    pointer type, last;
    do
    {
	ident_list(lastfield, &number, FIELD);
	type = type_denoter(FALSE);
	num = number;
	last = *lastfield;
	*length += num * typelength(type);
	while (num-- > 0)
	{
	    last->_Fld = (fieldpointer) EC_malloc(sizeof(struct fieldrecord));
	    last->_Fld_IsTag = FALSE;
	    last->type = type;
	    last = last->previous;
	}
	if (symbol == SYM_SEMICOLON)
	    nextsymbol();
	else
	    break;
    } while (symbol == SYM_IDENTIFIER);
    field_addressing(startoffset, *length, *lastfield);
}

static void NEAR 
variant(pointer * lastfield, pointer tagfield, short startoffset, short *maxlength, short caselist[])
{
    short thislength = startoffset;
    case_constant_list(tagfield->type, caselist, 1);
    expect(SYM_COLON);
    expect(SYM_LEFTPARENTHESIS);
    field_list(lastfield, startoffset, &thislength);
    if (thislength > *maxlength)
	*maxlength = thislength;
    expect(SYM_RIGHTPARENTHESIS);
}

static void NEAR 
variant_part(pointer * lastfield, short startoffset, short *length)
{
    pointer tagtype, tagfield;
    short ident, caselist[MAXCASEINDICES], i, maxlength = 0;
    nextsymbol();		/* skip CASE */
    ident = expect_ident();
    if (symbol == SYM_COLON)	/* Got tag field */
    {
	short len;
	nextsymbol();
	tagfield = define(ident, FIELD);	/* define the tag field */
	tagtype = type_denoter(TRUE);	/* get tag field type */
	tagfield->place = startoffset;
	len = typelength(tagtype);
	*length += len;
	startoffset += len;
    } else
    {
	tagfield = define(NOIDENTIFIER, FIELD);
	tagtype = find(ident);
    }
    if (isin(tagtype, ORDINALS))
    {
	tagfield->_Fld_IsTag = TRUE;
	tagfield->_Fld_IsTagd = FALSE;
	tagfield->type = tagtype;
    } else
	compiler_error(117, ERR_ORDINAL);
    expect(SYM_OF);
    for (i = 0; i < MAXCASEINDICES; i++)
	caselist[i] = 0;
    do
    {
	variant(lastfield, tagfield, startoffset, &maxlength, caselist);
	expect(SYM_SEMICOLON);
    } while (symbol != SYM_END);
    *length += maxlength;
    /*
     * Check that each case is dealt with; ie that caselist contains every
     * possible value in tagfield->type...
     */
}

static void NEAR 
field_list(pointer * lastfield, short startoffset, short *length)
{
    if (symbol == SYM_IDENTIFIER)
	fixed_part(lastfield, startoffset, length);
    if (symbol == SYM_CASE)
	variant_part(lastfield, startoffset + (*length), length);
}

static pointer NEAR 
new_recordtype(void)
{
    pointer lastfield, type;
    short length = 0;
    new_block(FALSE);
    nextsymbol();		/* Skip SYM_RECORD */
    field_list(&lastfield, 0, &length);
    exitblock();
    type = define(NOIDENTIFIER, RECORDTYPE);
    type->_Rec = (recpointer) EC_malloc(sizeof(struct recrecord));
    type->_Rec_LastFld = lastfield;
    type->_Rec_Len = length;
    expect(SYM_END);
    return type;
}

/*************************************************/

pointer 
ordinal_type(void)
{
    pointer rtn;
    if (symbol == SYM_IDENTIFIER && ((rtn = find_definition(argument)) != NULL))
    {
	if (!isin(rtn, ORDINALS))
	    compiler_error(123, ERR_ORDINAL);
	else
	    nextsymbol();
    } else
	rtn = new_ordinal_type();
    return rtn;
}


static pointer NEAR 
new_set_type(void)
{
    pointer rtn = define(NOIDENTIFIER, UNDEFINED);
    nextsymbol();		/* skip SYM_SET */
    expect(SYM_OF);
    rtn->kind = SETTYPE;
    rtn->type = ordinal_type();
    return rtn;
}


static pointer NEAR 
new_pointer_type(void)
{
    pointer type, object;
    type = define(NOIDENTIFIER, UNDEFINED);
    nextsymbol();		/* skip SYM_HAT */
    if ((object = find_definition(argument)) == NULL)
	object = define(argument, UNDEFINED);
    expect(SYM_IDENTIFIER);
    type->kind = POINTERTYPE;
    type->type = object;
    return type;
}


static pointer NEAR 
new_type(void)
{
    pointer type = 0;
    switch (symbol)
    {
    case SYM_PLUS:
    case SYM_MINUS:
    case SYM_IDENTIFIER:
    case SYM_INTEGER:
    case SYM_GRAPHIC:
    case SYM_LEFTPARENTHESIS:
	type = new_ordinal_type();
	break;
    case SYM_ARRAY:
	type = new_arraytype();
	break;
    case SYM_RECORD:
	type = new_recordtype();
	break;
    case SYM_SET:
	type = new_set_type();
	break;
    case SYM_HAT:
	type = new_pointer_type();
	break;
    default:
	break;
    }
    return type;
}

/****************************************************************/
/* ParameterList ::- ValueParameterSpecification		*/
/*		    [ ";" ValueParameterSpecification ]^0.	*/
/****************************************************************/

short 
    parameter_list(enum class kind, pointer * lastparam)
    {
	short length = value_parameter_specification(kind, lastparam);
	while (symbol == SYM_SEMICOLON)
	{
	    nextsymbol();
	    length += value_parameter_specification(kind, lastparam);
	}
	    return length;
    }

/*****************************/
/* PointerSelector ::- "^". */
/*****************************/

static void NEAR 
pointer_selector(pointer * type)
{
    nextsymbol();		/* Skip SYM_HAT */
    if (isin(*type, POINTERS))
    {
	emit(OP_VALUE, 1);	/* push(0); */
	*type = (*type)->type;
    } else
	compiler_error(125, ERR_TYPE);
}

/************************************************************************/
/* FunctionIdentifier ::- Identifier.					*/
/* FunctionBlock ::- Block.						*/
/* FunctionIdentification ::- "function" FunctionIdentifier		*/
/* FunctionDeclaration ::- FunctionHeading ";" Directive		*/
/*			| FunctionHeading ";" FunctionBlock		*/
/*			| FunctionIdentification ";" FunctionBlock.	*/
/* ProcedureIdentifier ::- Identifier.					*/
/* ProcedureBlock ::- Block.						*/
/* ProcedureIdentification ::- "procedure" ProcedureIdentifier		*/
/* ProcedureDefinition ::- ProcedureHeading ";" Directive		*/
/*			| ProcedureHeading ";" ProcedureBlock		*/
/*			| ProcedureIdentification ";" ProcedureBlock.	*/
/************************************************************************/

static void NEAR 
routine_definition(BOOLEAN isfunc)
{
    pointer proc;
    short ident, paramlength, varlabel, templabel, beginlabel;
    short resultlength = 0;
    enum declarationstatus status;

    nextsymbol();		/* skip PROCEDURE or FUNCTION */
    ident = expect_ident();
    status = COMPLETE;
    if ((proc = find_definition(ident)) == NULL)
	proc = define(ident, PROCEDUR);
    else
    {
	if (proc->kind == PROCEDUR)
	{
	    if ((status = proc->_Proc_Status) != PARTIAL)
		proc = define(ident, PROCEDUR);
	} else
	    proc = define(ident, PROCEDUR);
    }
    if (proc->_Proc == NULL)
	proc->_Proc = (procpointer) EC_malloc(sizeof(struct procrecord));
    proc->_Proc_Status = COMPLETE;
    proc->_Proc_Level = blocklevel;
    proc->_Proc_Label = new_label();
    proc->_Proc_IsFunc = isfunc;
    proc->_Proc_CanAsgn = isfunc;
    new_block(FALSE);
    if (symbol == SYM_LEFTPARENTHESIS)
    {
	if (status == PARTIAL)
	    compiler_error(126, ERR_PROC);
	nextsymbol();
	formal_parameter_list(&proc->_Proc_LastPar, &paramlength);
	expect(SYM_RIGHTPARENTHESIS);
    } else
    {
	proc->_Proc_LastPar = NULL;
	paramlength = 0;
    }
    if (isfunc)
    {
	if (symbol == SYM_COLON)
	{
	    if (status == PARTIAL)
		compiler_error(127, ERR_FUNC);
	    nextsymbol();
	    resultlength = typelength(proc->type = type_denoter(FALSE));
	} else			/* This should probably be replaced by an
				 * ERROr */
	    proc->type = typeuniversal;
	proc->place = -resultlength;
    }
    parameter_addressing(paramlength, proc->_Proc_LastPar, resultlength);
    varlabel = new_label();
    templabel = new_label();
    beginlabel = new_label();
    emit(OP_DEFADDR, proc->_Proc_Label);
    /* Procedures and functions are treated separately, so no PUSH/POPs */
    emit(OP_PROCEDURE, varlabel, templabel, beginlabel);
    push(paramlength);
    expect(SYM_SEMICOLON);
    if (symbol == SYM_PRIMITIVE || symbol == SYM_EXTERNAL)
	compiler_error(300, ERR_PRIMITIVE);
    else if (symbol == SYM_FORWARD)
    {
	proc->_Proc_Status = PARTIAL;
	nextsymbol();
    } else
	blockx(beginlabel, varlabel, templabel);
    expect(SYM_SEMICOLON);
    emit(OP_ENDPROC, paramlength, resultlength);
    pop(paramlength + 3 + resultlength);
    exitblock();
    if (isfunc)
	proc->_Proc_CanAsgn = FALSE;
}

void 
proc_and_func_decl_part(void)
{
    while (symbol == SYM_PURE || symbol == SYM_PROCEDURE || symbol == SYM_FUNCTION)
    {
	if (symbol == SYM_PURE)
	    nextsymbol();
	routine_definition((BOOLEAN) (symbol == SYM_FUNCTION));
    }
}

/****************************************************************/
/* ProcedureHeading ::- "procedure" Identifier			*/
/*			[ "(" FormalParameterList ")" ].	*/
/* ProceduralParameterSpecification ::- ProcedureHeading.	*/
/****************************************************************/

static void NEAR 
proc_parameter_specification(void)
{
    compiler_error(200, ERR_PROCPARAM);
}

/************************************************************************/
/* TypeDefinition ::- TypeIdentifier "=" ( TypeDenoter | "..." ).	*/
/* TypeDefinitionPart ::-  "type" [ TypeDefinition ";" ]^1 . 		*/
/* TypeDenoter ::- TypeIdentifier | NewType.				*/
/* TypeIdentifier ::- Identifier.					*/
/************************************************************************/

static void NEAR 
type_definition(void)
{
    pointer object, type;
    short ident = expect_ident();
    expect(SYM_EQUAL);
    if (symbol == SYM_TRIPLEDOT)
	compiler_error(225, ERR_ANYTYPE);
    else
    {
	object = type_denoter(FALSE);
	if (object->kind == DERIVEDTYPE)
	    object = object->type;
	if ((type = find_definition(ident)) == NULL)
	{
	    if (object->kind == STANDARDTYPE)
	    {
		type = define(ident, DERIVEDTYPE);
		type->type = object;
	    } else if (object->ident != NOIDENTIFIER)
	    {
		type = define(NOIDENTIFIER, UNDEFINED);
		type = object;
		type->ident = ident;
	    } else
		object->ident = ident;
	} else if (type->kind != UNDEFINED)
	    compiler_error(130, ERR_AMBIGUOUS);
	else
	{
	    type = object;
	    type->ident = ident;
	}
    }
}

void 
type_definition_part(void)
{
    expect(SYM_TYPE);
    type_definition();
    expect(SYM_SEMICOLON);
    while (symbol == SYM_IDENTIFIER)
    {
	type_definition();
	expect(SYM_SEMICOLON);
    }
}

/* In the next function 'param' should be set TRUE for parameter and
	variant tag types; this forces type_idents instead of the
	usual type_denoter. */

static pointer NEAR 
type_denoter(BOOLEAN param)
{
    pointer type = 0;
    BOOLEAN done = FALSE;
    if (symbol == SYM_IDENTIFIER)
    {
	type = find(argument);
	switch (type->kind)
	{
	case DERIVEDTYPE:
	    type = type->type;
	case STANDARDTYPE:
	case ARRAYTYPE:
	case RECORDTYPE:
	case SETTYPE:
	case SUBRANGETYPE:
	case ENUMERATEDTYPE:
	case POINTERTYPE:
	    done = TRUE;
	default:
	    break;
	}
    }
    if (param)
	expect(SYM_IDENTIFIER);	/* cause syntax error if not done... */
    else
    {
	if (done)
	    nextsymbol();	/* skip type identifier */
	else
	    type = new_type();	/* new implicit type declaration */
    }
    return type;
}

/****************************************************************/
/* FieldDesignatorIdentifier ::- Identifier. 			*/
/* FieldDesignator ::- FieldDesignatorIdentifier. 		*/
/* ArrayVariable ::- VariableAccess. 				*/
/* RecordVariable ::- VariableAccess. 				*/
/* ComponentVariable ::- ArrayVariable IndexedSelector 		*/
/*		      | RecordVariable FieldSelector. 		*/
/*		      | FieldDesignator 			*/
/*		      | ExportedVariable. 			*/
/* PointerVariable ::- VariableAccess. 				*/
/* IdentifiedVariable ::- PointerVariable PointerSelector. 	*/
/* VariableIdentifier ::- Identifier. 				*/
/* EntireVariable ::- VariableIdentifier. 			*/
/* VariableAccess ::- EntireVariable 				*/
/*		   | ComponentVariable 				*/
/*		   | IdentifiedVariable. 			*/
/****************************************************************/

pointer 
variable_access(BOOLEAN reference, BOOLEAN is_exported)
{
    pointer type, object;
    short level, displ;
    if (symbol == SYM_IDENTIFIER)
    {
	object = find(argument);
	switch (object->kind)
	{
	case FIELD:
	    expect(SYM_IDENTIFIER);
	    type = object->type;
	    emit(OP_WITHFIELD, object->place, object->_Fld_WithLevel);
	    break;
	case VARIABLE:
	case VALUEPARAMETER:
	case VARPARAMETER:
	case INTERACTIONARG:
	    expect(SYM_IDENTIFIER);
	    if (reference && object->_Var_IsLoop)
		compiler_error(132, ERR_LOOPVAR);
	    type = object->type;
	    level = blocklevel - object->_Var_Level;
	    displ = object->place;
	    if (is_exported)
	    {
		if (object->kind == VARIABLE)
		    if (object->_Var_IsExp)
			emit(OP_EXPVAR, level, displ);
		    else
			compiler_error(132, ERR_NOTEXPORTED);
		else
		    compiler_error(132, ERR_BADEXPORT);
	    } else if (object->kind == VARPARAMETER)
		emit(OP_VARPARAM, level, displ);
	    else
		emit(OP_VARIABLE, level, displ);
	    break;
	case MODVAR:		/* this does its own indexing */
	    if (is_exported)
		compiler_error(133, ERR_BADEXPORT);
	    /*
	     * If we are doing a module variable assignment, we want the
	     * modvar ident for the lvalue only; thus we pass 'reference' as
	     * the boolean flag for symbolic info to module_variable. The
	     * latter actually only produces the modvar ident if the follow
	     * symbol is not SYM_PERIOD, as in this case we have an exported
	     * variable assignment, NOT a module var assignment.
	     */
	    type = module_variable(reference);
	    if (symbol == SYM_PERIOD)
	    {
		/*
		 * Must temporarily link in module header symbol table
		 * entries
		 */
		new_block(block[blocklevel].estelleblock);
		block[blocklevel].lastobject = type->_Mod_LastEntry;
		nextsymbol();
		type = variable_access(reference, TRUE);
		exitblock();
	    }
	    break;
	default:
	    expect(SYM_IDENTIFIER);
	    compiler_error(133, ERR_TYPE);
	    break;
	}
	while (symbol == SYM_HAT || symbol == SYM_LEFTBRACKET || symbol == SYM_PERIOD)
	{
	    if (symbol == SYM_LEFTBRACKET)
		indexed_selector(&type);
	    else if (symbol == SYM_HAT)
		pointer_selector(&type);
	    else
		field_selector(&type);
	}
    } else
	compiler_error(134, ERR_NOIDENT);
    return type;
}

/****************************************************************/
/* VariableDeclaration ::- IdentifierList ":" TypeDenoter.	*/
/* VariableDeclaration ::- VariableGroup.			*/
/****************************************************************/

short 
variable_declaration(pointer * lastvar)	/* check level 1 */
{
    short number;
    pointer type = variable_group(VARIABLE, &number, lastvar, FALSE);
    return number * typelength(type);
}

/********************************************************************/
/* VariableDeclarationPart ::- "var" [ VariableDeclaration ";" ]^1. */
/********************************************************************/

short 
variable_declaration_part(short start_len)	/* check level 1 */
{
    pointer lastvar;
    short length;
    nextsymbol();
    length = start_len + variable_declaration(&lastvar);
    expect(SYM_SEMICOLON);
    while (symbol == SYM_IDENTIFIER)
    {
	length += variable_declaration(&lastvar);
	expect(SYM_SEMICOLON);
    }
    variable_addressing(length, lastvar);
    while (lastvar)
    {
	if (lastvar->kind == POINTERTYPE)
	    if (lastvar->type->kind == UNDEFINED)
		lastvar->type = find_definition(lastvar->type->ident);
	lastvar = lastvar->previous;
    }
    return length;
}

/************************************************************************/
/* VariableGroup ::- VariableIdentifier GroupTail.			*/
/* GroupTail ::- "," VariableGroup | ":" TypeDenoter.			*/
/* VariableGroup2 ::- VariableIdentifier Group2Tail.			*/
/* Group2Tail ::- "," VariableGroup2 | ":" TypeIdentifier.		*/
/* ValueParameterSpecification ::- IdentifierList ":" TypeIdentifier.	*/
/* ValueParameterSpecification ::- VariableGroup2.			*/
/* VariableParameterSpecification ::- "var" IdentifierList		*/
/*					":" TypeIdentifier.		*/
/************************************************************************/

static pointer 
    variable_group(enum class kind, short *number, pointer * lastvar, BOOLEAN param)
    {
	pointer type, last;
	short num;
	do
	{
	    ident_list(lastvar, number, kind);
	    type = param ? type_denoter(TRUE) : type_denoter(FALSE);
	    last = *lastvar;
	    num = *number;
	    while (num-- > 0)
	    {
		last->type = type;
		last->_Var = (varpointer) EC_malloc(sizeof(struct varrecord));
		last->_Var_IsLoop = FALSE;
		last->_Var_IsExp = FALSE;
		last = last->previous;
	    }
	} while (symbol == SYM_IDENTIFIER);
	return type;
    }

static short NEAR 
    value_parameter_specification(enum class kind, pointer * lastparam)
    {
	short number;
	pointer type;
	    type = variable_group(kind, &number, lastparam, TRUE);
	    return typelength(type) * number;
    }

    static void NEAR variable_parameter_specification(pointer * lastparam, short *number)
{
    expect(SYM_VAR);
    (void)variable_group(VARPARAMETER, number, lastparam, TRUE);
}
