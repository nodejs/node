//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

#ifdef IR_VIEWER


/* ----- PRIVATE ----- */

Js::DynamicObject * IRtoJSObjectBuilder::CreateIntConstOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd)
{
    if (!opnd || !opnd->IsIntConstOpnd())
    {
        return NULL;
    }

    IR::IntConstOpnd *op = opnd->AsIntConstOpnd();
    IntConstType value = op->GetValue();

    Js::Var valueVar = Js::JavascriptNumber::ToVar(value, scriptContext);

    Js::DynamicObject *opObject = scriptContext->GetLibrary()->CreateObject();
    SetProperty(opObject, L"value", valueVar);

    return opObject;
}

Js::DynamicObject * IRtoJSObjectBuilder::CreateFloatConstOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd)
{
    if (!opnd || !opnd->IsFloatConstOpnd())
    {
        return NULL;
    }

    IR::FloatConstOpnd *op = opnd->AsFloatConstOpnd();
    FloatConstType value = op->m_value;

    Js::Var valueVar = Js::JavascriptNumber::New(value, scriptContext);

    Js::DynamicObject *opObject = scriptContext->GetLibrary()->CreateObject();
    SetProperty(opObject, L"value", valueVar);

    return opObject;
}

Js::DynamicObject * IRtoJSObjectBuilder::CreateHelperCallOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd)
{
    if (!opnd || !opnd->IsHelperCallOpnd())
    {
        return NULL;
    }

    IR::HelperCallOpnd *op = opnd->AsHelperCallOpnd();

    const wchar_t *helperText = IR::GetMethodName(op->m_fnHelper);
    Js::JavascriptString *helperString = NULL;
    helperString = Js::JavascriptString::NewCopyBuffer(helperText, wcslen(helperText), scriptContext);

    Js::DynamicObject *opObject = scriptContext->GetLibrary()->CreateObject();
    SetProperty(opObject, L"methodName", helperString);

    return opObject;
}

Js::DynamicObject * IRtoJSObjectBuilder::CreateSymOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd)
{
    if (!opnd || !opnd->IsSymOpnd())
    {
        return NULL;
    }

    IR::SymOpnd *op = opnd->AsSymOpnd();
    SymID id = op->m_sym->m_id;

    Js::Var idValue = Js::JavascriptNumber::ToVar(id, scriptContext);

    Js::DynamicObject *opObject = scriptContext->GetLibrary()->CreateObject();
    SetProperty(opObject, L"symid", idValue);

    return opObject;
}

Js::DynamicObject * IRtoJSObjectBuilder::CreateRegOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd)
{
    if (!opnd || !opnd->IsRegOpnd())
    {
        return NULL;
    }

    Js::DynamicObject *opObject = scriptContext->GetLibrary()->CreateObject();
    IR::RegOpnd *op = opnd->AsRegOpnd();

    RegNum regid = op->GetReg();
    SymID symid = 0;

    if (op->m_sym)
    {
        symid = op->m_sym->m_id;
    }

    if (regid != RegNOREG)
    {
        Js::Var regidValue = Js::JavascriptNumber::ToVar(regid, scriptContext);
        SetProperty(opObject, L"regid", regidValue);
    }

    Js::Var symidValue = Js::JavascriptNumber::ToVar(symid, scriptContext);
    SetProperty(opObject, L"symid", symidValue);

    return opObject;
}

Js::DynamicObject * IRtoJSObjectBuilder::CreateAddrOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd, Func *func)
{
    if (!opnd || !opnd->IsAddrOpnd())
    {
        return NULL;
    }

    IR::AddrOpnd *op = opnd->AsAddrOpnd();
    Js::Var address = op->m_address;  // TODO (doilij) see opnd.cpp:1802 - not always m_address
    Js::Var addressVar = Js::JavascriptNumber::ToVar((uint64)address, scriptContext);

    Js::DynamicObject *opObject = scriptContext->GetLibrary()->CreateObject();
    SetProperty(opObject, L"addr", addressVar);

    const size_t count = BUFFER_LEN;
    wchar_t detail[count];
    op->GetAddrDescription(detail, count, false, false, func);

    Js::JavascriptString *detailString = NULL;
    detailString = Js::JavascriptString::NewCopyBuffer(detail, wcslen(detail), scriptContext);
    SetProperty(opObject, L"detail", detailString);

    return opObject;
}

Js::DynamicObject * IRtoJSObjectBuilder::CreateIndirOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd)
{
    if (!opnd || !opnd->IsIndirOpnd())
    {
        return NULL;
    }

    IR::IndirOpnd *op = opnd->AsIndirOpnd();
    Js::DynamicObject *opObject = scriptContext->GetLibrary()->CreateObject();

    IR::RegOpnd *baseOpnd = op->GetBaseOpnd();

    Js::Var baseVar = NULL;
    if (baseOpnd->m_sym) {
        SymID baseid = baseOpnd->m_sym->m_id;
        baseVar = Js::JavascriptNumber::ToVar(baseid, scriptContext);
    } else {
        RegNum regid = baseOpnd->GetReg();
        baseVar = Js::JavascriptNumber::ToVar(regid, scriptContext);
    }

    SetProperty(opObject, L"base", baseVar);

    IR::RegOpnd *indexOpnd = op->GetIndexOpnd();
    if (indexOpnd)
    {
        SymID indexid = indexOpnd->m_sym->m_id;
        Js::Var indexVar = Js::JavascriptNumber::ToVar(indexid, scriptContext);
        SetProperty(opObject, L"index", indexVar);
    }

    int32 offset = op->GetOffset();
    if (offset)
    {
        Js::Var offsetVar = Js::JavascriptNumber::ToVar(offset, scriptContext);
        SetProperty(opObject, L"offset", offsetVar);
    }

    return opObject;
}

Js::DynamicObject * IRtoJSObjectBuilder::CreateLabelOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd)
{
    if (!opnd || !opnd->IsLabelOpnd())
    {
        return NULL;
    }

    /*
    TODO (doilij) unneeded method?

    I don't think any code path would reach here.

    For Branch and Label, the LabelOpnd's are taken care of by other code paths:
    IRtoJSObjectBuilder::CreateLabelInstruction (IRtoJSObject.cpp:281)
    IRtoJSObjectBuilder::CreateBranchInstruction (IRtoJSObject.cpp:293)
    */

    Js::DynamicObject *opObject = scriptContext->GetLibrary()->CreateObject();
    return opObject;
}


/* ----- PUBLIC ----- */


Js::DynamicObject * IRtoJSObjectBuilder::CreateOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd, Func *func)
{
    if (!opnd)
    {
        return NULL;
    }

    Js::DynamicObject *opObject = NULL;

    IR::OpndKind kind = opnd->GetKind();
    switch (kind)
    {

    case IR::OpndKind::OpndKindInvalid:
        // do nothing
        break;

    case IR::OpndKind::OpndKindIntConst:
        opObject = CreateIntConstOpnd(scriptContext, opnd);
        break;

    case IR::OpndKind::OpndKindFloatConst:
        opObject = CreateFloatConstOpnd(scriptContext, opnd);
        break;

    case IR::OpndKind::OpndKindHelperCall:
        opObject = CreateHelperCallOpnd(scriptContext, opnd);
        break;

    case IR::OpndKind::OpndKindSym:
        opObject = CreateSymOpnd(scriptContext, opnd);
        break;

    case IR::OpndKind::OpndKindReg:
        opObject = CreateRegOpnd(scriptContext, opnd);
        break;

    case IR::OpndKind::OpndKindAddr:
        opObject = CreateAddrOpnd(scriptContext, opnd, func);
        break;

    case IR::OpndKind::OpndKindIndir:
        opObject = CreateIndirOpnd(scriptContext, opnd);
        break;

    case IR::OpndKind::OpndKindLabel:
        opObject = CreateLabelOpnd(scriptContext, opnd);
        break;

    case IR::OpndKind::OpndKindMemRef:
        // TODO (doilij) implement
        break;

    case IR::OpndKind::OpndKindRegBV:
        // TODO (doilij) implement
        break;

    default:
        break;

    }

    // fallback create an object that can be used for identifying what info is missing
    if (!opObject)
    {
        opObject = scriptContext->GetLibrary()->CreateObject();
    }

    // include the kind of symbol so the UI knows how to display the information
    Js::Var kindVar = Js::JavascriptNumber::ToVar(kind, scriptContext);
    Js::PropertyId id_kind = CreateProperty(scriptContext, L"kind");
    SetProperty(opObject, id_kind, kindVar);

    return opObject;
}

Js::PropertyId IRtoJSObjectBuilder::CreateProperty(Js::ScriptContext *scriptContext, const wchar_t *propertyName)
{
    Js::PropertyRecord const *propertyRecord;
    scriptContext->GetOrAddPropertyRecord(propertyName, (int) wcslen(propertyName), &propertyRecord);
    Js::PropertyId propertyId = propertyRecord->GetPropertyId();

    return propertyId;
}

void IRtoJSObjectBuilder::SetProperty(Js::DynamicObject *obj, const wchar_t *propertyName, Js::Var value)
{
    const size_t len = wcslen(propertyName);
    if (!(len > 0))
    {
        return;
    }

    Js::PropertyId id = CreateProperty(obj->GetScriptContext(), propertyName);
    SetProperty(obj, id, value);
}

void IRtoJSObjectBuilder::SetProperty(Js::DynamicObject *obj, Js::PropertyId id, Js::Var value)
{
    if (value == NULL)
    {
        return;
    }

    Js::JavascriptOperators::SetProperty(obj, obj, id, value, obj->GetScriptContext());
}

enum STATEMENT_PARSE_T {
    STATEMENT_PARSE_NORMAL,
    STATEMENT_PARSE_QUOT,
    STATEMENT_PARSE_QQUOT,
    STATEMENT_PARSE_ESC_QUOT,
    STATEMENT_PARSE_ESC_QQUOT,
    STATEMENT_PARSE_SEARCH_COMMENT,
    STATEMENT_PARSE_LINE_COMMENT,
    STATEMENT_PARSE_BLOCK_COMMENT,
    STATEMENT_PARSE_SEARCH_BLOCK_COMMENT_END,
    STATEMENT_PARSE_SEMICOLON,
    STATEMENT_PARSE_END,
    STATEMENT_PARSE_MAX
};

/**
    Get the statement's source string, cutting the source string short by detecting the first semicolon
    which is not contained within a comment or string.

    Uses a regular FSM to scan char-by-char for the end of the statement.

    NOTE: This does not account for the possibility of semicolons within RegEx. In order to properly
    detect where a RegEx begins and ends, a more sophisticated parser is probably needed.

    @param buffer           A buffer to write the source line into.
    @param sourceBegin      A pointer to the beginning of the source string.
    @param sourceEnd        A pointer to the farthest point at which the end of statement could exist.
    @param len              The size of the buffer (maximum number of characters to copy).
*/
void IRtoJSObjectBuilder::GetStatementSourceString(__out_ecount(len) wchar_t *buffer, LPCUTF8 sourceBegin, LPCUTF8 sourceEnd, const size_t len)
{
    enum STATEMENT_PARSE_T state;
    size_t i;

    // copy characters until end of statement is reached (semicolon or newline)
    for (i = 0, state = STATEMENT_PARSE_NORMAL;
        (i < len-1) && ((sourceBegin+i) < sourceEnd) && state != STATEMENT_PARSE_END;
        i++)
    {
        utf8char_t ch = sourceBegin[i];
        buffer[i] = ch;

        switch (state)
        {
        case STATEMENT_PARSE_NORMAL:
            switch (ch)
            {
            case '\'':
                state = STATEMENT_PARSE_QUOT;
                break;
            case '\"':
                state = STATEMENT_PARSE_QQUOT;
                break;
            case '/':
                state = STATEMENT_PARSE_SEARCH_COMMENT;
                break;
            case ';':
                state = STATEMENT_PARSE_END;
                break;
            default:
                // continue in this state
                break;
            }
            break;
        case STATEMENT_PARSE_QUOT:  // single quoted string
            switch (ch)
            {
            case '\\':
                state = STATEMENT_PARSE_ESC_QUOT;
                break;
            case '\'':
                state = STATEMENT_PARSE_NORMAL;  // end of single-quoted string
                break;
            default:
                // continue in this state
                break;
            }
            break;
        case STATEMENT_PARSE_QQUOT:  // double quoted string
            switch (ch)
            {
            case '\\':
                state = STATEMENT_PARSE_ESC_QQUOT;
                break;
            case '\"':
                state = STATEMENT_PARSE_NORMAL;  // end of double-quoted string
                break;
            default:
                // continue in this state
                break;
            }
            break;
        case STATEMENT_PARSE_ESC_QUOT:
            state = STATEMENT_PARSE_QUOT;  // unconditionally ignore this character and return to string parsing
            break;
        case STATEMENT_PARSE_ESC_QQUOT:
            state = STATEMENT_PARSE_QQUOT;  // unconditionally ignore this character and return to string parsing
            break;
        case STATEMENT_PARSE_SEARCH_COMMENT:
            switch (ch)
            {
            case '/':
                state = STATEMENT_PARSE_LINE_COMMENT;
                break;
            case '*':
                state = STATEMENT_PARSE_BLOCK_COMMENT;
                break;
            default:
                state = STATEMENT_PARSE_NORMAL;
                break;
            }
            break;
        case STATEMENT_PARSE_LINE_COMMENT:
            // do nothing till end of line
            break;
        case STATEMENT_PARSE_BLOCK_COMMENT:
            switch (ch)
            {
            case '*':
                state = STATEMENT_PARSE_SEARCH_BLOCK_COMMENT_END;
                break;
            default:
                // continue in this state
                break;
            }
            break;
        case STATEMENT_PARSE_SEARCH_BLOCK_COMMENT_END:
            switch (ch)
            {
            case '/':
                state = STATEMENT_PARSE_NORMAL;
                break;
            default:
                // continue in this state
                break;
            }
            break;
        }
    }

    Assert(i < len);
    buffer[i] = 0;  // NULL terminate
}

void IRtoJSObjectBuilder::CreateLabelInstruction(Js::ScriptContext *scriptContext,
                                                 IR::LabelInstr *inst, Js::DynamicObject *currObject)
{
    Js::Var labelVar = Js::JavascriptNumber::ToVar(inst->m_id, scriptContext);
    SetProperty(currObject, L"label", labelVar);
}

void IRtoJSObjectBuilder::CreateBranchInstruction(Js::ScriptContext *scriptContext,
                                                  IR::BranchInstr *inst, Js::DynamicObject *currObject)
{
    if (!inst->GetTarget())
    {
        return;
    }

    Js::Var labelVar = Js::JavascriptNumber::ToVar(inst->GetTarget()->m_id, scriptContext);
    SetProperty(currObject, L"branch", labelVar);
}

void IRtoJSObjectBuilder::CreatePragmaInstruction(Js::ScriptContext *scriptContext,
                                                  IR::PragmaInstr *inst, Js::DynamicObject *currObject,
                                                  Func *func)
{
    int32 statementIndex = (int32)inst->m_statementIndex;
    LPCUTF8 sourceBegin = NULL;
    LPCUTF8 sourceEnd = NULL;
    ULONG line = 0;
    LONG col = 0;

    Js::FunctionBody *fnBody = func->GetJnFunction()->GetFunctionBody();
    fnBody->GetStatementSourceInfo(statementIndex, &sourceBegin, &sourceEnd, &line, &col);

    // move line and column into a sane range
    line += 2;      // start at line 1 (up from -1)
    col += 1;       // start at col 1 (up from 0)

    //
    // extract source string
    //

    wchar_t buffer[BUFFER_LEN];
    GetStatementSourceString(buffer, sourceBegin, sourceEnd, BUFFER_LEN);

    //
    // assign source info
    //

    Js::JavascriptString *sourceString = NULL;
    sourceString = Js::JavascriptString::NewCopyBuffer(buffer, wcslen(buffer), scriptContext);

    SetProperty(currObject, L"source", sourceString);

    Js::Var lineVar = Js::JavascriptNumber::ToVar((uint32)line, scriptContext);
    SetProperty(currObject, L"line", lineVar);

    Js::Var colVar = Js::JavascriptNumber::ToVar((uint32)col, scriptContext);
    SetProperty(currObject, L"col", colVar);

    if (statementIndex != -1)
    {
        Js::Var indexVar = Js::JavascriptNumber::ToVar(statementIndex, scriptContext);
        SetProperty(currObject, L"statementIndex", indexVar);
    }
}

void IRtoJSObjectBuilder::CreateDefaultInstruction(Js::ScriptContext *scriptContext,
                                                   IR::Instr *currInst, Js::DynamicObject *currObject,
                                                   Func *func)
{
    Js::DynamicObject *src1 = IRtoJSObjectBuilder::CreateOpnd(scriptContext, currInst->GetSrc1(), func);
    Js::DynamicObject *src2 = IRtoJSObjectBuilder::CreateOpnd(scriptContext, currInst->GetSrc2(), func);
    Js::DynamicObject *dst = IRtoJSObjectBuilder::CreateOpnd(scriptContext, currInst->GetDst(), func);

    SetProperty(currObject, L"src1", src1);
    SetProperty(currObject, L"src2", src2);
    SetProperty(currObject, L"dst", dst);
}

/**
    Retrieve metadata about the IR dump.

    Includes the following information:
    * The names of registers (for each regid) for the current architecture.
*/
Js::DynamicObject * IRtoJSObjectBuilder::GetMetadata(Js::ScriptContext *scriptContext)
{
    Js::JavascriptArray *regNameArray = scriptContext->GetLibrary()->CreateArray(RegNumCount);
    for (int i = 0; i < RegNumCount; ++i)
    {
        const wchar_t *regName = RegNamesW[i];
        Js::Var detailString;
        detailString = Js::JavascriptString::NewCopyBuffer(regName, wcslen(regName), scriptContext);
        // regNameArray->SetArrayLiteralItem(i, detailString);
        regNameArray->SetItem(i, detailString, Js::PropertyOperationFlags::PropertyOperation_None);
    }

    Js::DynamicObject *metadata = scriptContext->GetLibrary()->CreateObject();
    SetProperty(metadata, L"regNames", regNameArray);

    return metadata;
}

Js::DynamicObject * IRtoJSObjectBuilder::DumpIRtoJSObject(Func *func, Js::Phase phase)
{
    if (!CONFIG_FLAG(IRViewer) || !func)
    {
        // if we actually return null and this value is used, we end up with a failed assertion
        return NULL;  // TODO (doilij) is this okay or does the return value need to be explicitly undefined?
    }

#ifdef ENABLE_IR_VIEWER_DBG_DUMP
    if (Js::Configuration::Global.flags.Verbose)
    {
        Output::Print(L"\n>>> Begin Dump IR to JS Object (phase: %s) <<<\n", Js::PhaseNames[phase]);
        Output::Flush();
    }
#endif

    // FIXME (doilij) why only printing the last function? because linking pointer to first IR stmt for a function
    // TODO (doilij) make a linked list of functions instead which contain a linked list of IR statements

    CodeGenWorkItem *workItem = func->m_workItem;
    Js::ScriptContext *scriptContext = workItem->irViewerRequestContext;
    if (!scriptContext)
    {
        // TODO (doilij) should set the requestContext on parseIR code path
        scriptContext = func->GetScriptContext();
    }

    Js::DynamicObject *dumpir = scriptContext->GetLibrary()->CreateObject();

    Js::DynamicObject *prevObject = NULL;
    Js::DynamicObject *currObject = dumpir;
    IR::Instr *currInst = func->m_headInstr;

    // property ids for loop
    Js::PropertyId id_opcode = CreateProperty(scriptContext, L"opcode");
    Js::PropertyId id_next = CreateProperty(scriptContext, L"next");

    while (currInst)
    {
        //
        // "currObject.opcode = Js::OpCodeNamesW[opcode]"
        //

        Js::OpCode opcode = currInst->m_opcode;
        wchar_t const *const opcodeName = Js::OpCodeUtil::GetOpCodeName(opcode);

        Js::JavascriptString *opcodeString = NULL;
        opcodeString = Js::JavascriptString::NewCopyBuffer(opcodeName, wcslen(opcodeName), scriptContext);

        SetProperty(currObject, id_opcode, opcodeString);

        //
        // operands
        //

        if (currInst->IsLabelInstr())
        {
            IR::LabelInstr *inst = currInst->AsLabelInstr();
            CreateLabelInstruction(scriptContext, inst, currObject);
        }
        else if (currInst->IsBranchInstr())
        {
            IR::BranchInstr *inst = currInst->AsBranchInstr();
            CreateBranchInstruction(scriptContext, inst, currObject);
        }
        else if (currInst->IsPragmaInstr())
        {
            IR::PragmaInstr *inst = currInst->AsPragmaInstr();
            CreatePragmaInstruction(scriptContext, inst, currObject, currInst->m_func);
        }
        else
        {
            CreateDefaultInstruction(scriptContext, currInst, currObject, func);
        }

        //
        // "prevObject.next = currObject"
        //

        if (prevObject)
        {
            SetProperty(prevObject, id_next, currObject);
            // Note: setting a prev pointer may cause an infinite loop when converting to JSON
        }

        //
        // epilogue
        //

        prevObject = currObject;
        currObject = scriptContext->GetLibrary()->CreateObject();

        currInst = currInst->m_next;
    }

    Js::DynamicObject *baseObject = func->GetJnFunction()->GetFunctionBody()->GetIRDumpBaseObject();

    // attach output for given phase
    SetProperty(baseObject, Js::PhaseNames[phase], dumpir);

    // attach IR metadata
    Js::PropertyId id_metadata = CreateProperty(scriptContext, L"metadata");
    if (!baseObject->HasProperty(id_metadata))
    {
        Js::DynamicObject *metadata = GetMetadata(scriptContext);
        SetProperty(baseObject, id_metadata, metadata);
    }

    return baseObject;
}

// TODO (doilij) update the name of this function
// TODO (doilij) write documentation for this function
void IRtoJSObjectBuilder::DumpIRtoGlobalObject(Func *func, Js::Phase phase)
{
    // FIXME (doilij) this cast has got to be unnecessary
    CodeGenWorkItem *workItem =func->m_workItem;
    bool rejit = workItem->isRejitIRViewerFunction;
    bool irDumpEnabled = func->GetJnFunction()->GetFunctionBody()->IsIRDumpEnabled();
    if (!(irDumpEnabled || rejit))
    {
        return;
    }

    Js::DynamicObject *baseObject = DumpIRtoJSObject(func, phase);
    workItem->SetIRViewerOutput(baseObject);
}

#endif /* IR_VIEWER */
