//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"


#if ENABLE_DEBUG_CONFIG_OPTIONS
#define BAILOUT_VERBOSE_TRACE(functionBody, ...) \
    if (Js::Configuration::Global.flags.Verbose && Js::Configuration::Global.flags.Trace.IsEnabled(Js::BailOutPhase,functionBody->GetSourceContextId(),functionBody->GetLocalFunctionId())) \
    { \
        Output::Print(__VA_ARGS__); \
    }

#define BAILOUT_FLUSH(functionBody) \
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::BailOutPhase, functionBody->GetSourceContextId(),functionBody->GetLocalFunctionId()) || \
    Js::Configuration::Global.flags.Trace.IsEnabled(Js::BailOutPhase, functionBody->GetSourceContextId(),functionBody->GetLocalFunctionId())) \
    { \
        Output::Flush(); \
    }
#else
#define BAILOUT_VERBOSE_TRACE(functionBody, bailOutKind, ...)
#define BAILOUT_FLUSH(functionBody)
#endif

void BailoutConstantValue::InitVarConstValue(Js::Var value)
{
    this->type = TyVar;
    this->u.varConst.value = value;
}

Js::Var BailoutConstantValue::ToVar(Func* func, Js::ScriptContext* scriptContext) const
{
    Assert(this->type == TyVar || this->type == TyFloat64 || IRType_IsSignedInt(this->type));
    Js::Var varValue;
    if (this->type == TyVar)
    {
        varValue = this->u.varConst.value;
    }
    else if (this->type == TyFloat64)
    {
        varValue = Js::JavascriptNumber::NewCodeGenInstance(func->GetNumberAllocator(), (double)this->u.floatConst.value, scriptContext);
    }
    else if (IRType_IsSignedInt(this->type) && TySize[this->type] <= 4 && !Js::TaggedInt::IsOverflow((int32)this->u.intConst.value))
    {
        varValue = Js::TaggedInt::ToVarUnchecked((int32)this->u.intConst.value);
    }
    else
    {
        varValue = Js::JavascriptNumber::NewCodeGenInstance(func->GetNumberAllocator(), (double)this->u.intConst.value, scriptContext);
    }
    return varValue;

}


void InlineeFrameInfo::AllocateRecord(Func* func, Js::FunctionBody* functionBody)
{
    uint constantCount = 0;

    // If there are no helper calls there is a chance that frame record is not required after all;
    arguments->Map([&](uint index, InlineFrameInfoValue& value){
        if (value.IsConst())
        {
            constantCount++;
        }
    });

    if (function.IsConst())
    {
        constantCount++;
    }

    // For InlineeEnd's that have been cloned we can result in multiple calls
    // to allocate the record - do not allocate a new record - instead update the existing one.
    // In particular, if the first InlineeEnd resulted in no calls and spills, subsequent might still spill - so it's a good idea to
    // update the record
    if (!this->record)
    {
        this->record = InlineeFrameRecord::New(func->GetNativeCodeDataAllocator(), (uint)arguments->Count(), constantCount, functionBody, this);
    }

    uint i = 0;
    uint constantIndex = 0;
    arguments->Map([&](uint index, InlineFrameInfoValue& value){
        Assert(value.type != InlineeFrameInfoValueType_None);
        if (value.type == InlineeFrameInfoValueType_Sym)
        {
            int offset;
#ifdef MD_GROW_LOCALS_AREA_UP
            offset = -((int)value.sym->m_offset + BailOutInfo::StackSymBias);
#else
            // Stack offset are negative, includes the PUSH EBP and return address
            offset = value.sym->m_offset - (2 * MachPtr);
#endif
            Assert(offset < 0);
            this->record->argOffsets[i] = offset;
            if (value.sym->IsFloat64())
            {
                this->record->floatArgs.Set(i);
            }
            else if (value.sym->IsInt32())
            {
                this->record->losslessInt32Args.Set(i);
            }
        }
        else
        {
            // Constants
            Assert(constantIndex < constantCount);
            this->record->constants[constantIndex] = value.constValue.ToVar(func, func->GetJnFunction()->GetScriptContext());
            this->record->argOffsets[i] = constantIndex;
            constantIndex++;
        }
        i++;
    });

    if (function.type == InlineeFrameInfoValueType_Sym)
    {
        int offset;

#ifdef MD_GROW_LOCALS_AREA_UP
        offset = -((int)function.sym->m_offset + BailOutInfo::StackSymBias);
#else
        // Stack offset are negative, includes the PUSH EBP and return address
        offset = function.sym->m_offset - (2 * MachPtr);
#endif
        this->record->functionOffset = offset;
    }
    else
    {
        Assert(constantIndex < constantCount);
        this->record->constants[constantIndex] = function.constValue.ToVar(func, func->GetJnFunction()->GetScriptContext());
        this->record->functionOffset = constantIndex;
    }
}

void InlineeFrameRecord::PopulateParent(Func* func)
{
    Assert(this->parent == nullptr);
    Assert(!func->IsTopFunc());
    if (func->GetParentFunc()->m_hasInlineArgsOpt)
    {
        this->parent = func->GetParentFunc()->frameInfo->record;
        Assert(this->parent != nullptr);
    }
}

void InlineeFrameRecord::Finalize(Func* inlinee, uint32 currentOffset)
{
    this->PopulateParent(inlinee);
    this->inlineeStartOffset = currentOffset;
    this->inlineDepth = inlinee->inlineDepth;

#ifdef MD_GROW_LOCALS_AREA_UP
    Func* topFunc = inlinee->GetTopFunc();
    int32 inlineeArgStackSize = topFunc->GetInlineeArgumentStackSize();
    int localsSize = topFunc->m_localStackHeight + topFunc->m_ArgumentsOffset;

    this->MapOffsets([=](int& offset)
    {
        int realOffset = -(offset + BailOutInfo::StackSymBias);
        if (realOffset < 0)
        {
            // Not stack offset
            return;
        }
        // The locals size contains the inlined-arg-area size, so remove the inlined-arg-area size from the
        // adjustment for normal locals whose offsets are relative to the start of the locals area.

        realOffset -= (localsSize - inlineeArgStackSize);
        offset = realOffset;
    });
#endif

    Assert(this->inlineDepth != 0);
}

void InlineeFrameRecord::Restore(Js::FunctionBody* functionBody, InlinedFrameLayout *inlinedFrame, Js::JavascriptCallStackLayout * layout) const
{
    Assert(this->inlineDepth != 0);
    Assert(inlineeStartOffset != 0);

    BAILOUT_VERBOSE_TRACE(functionBody, L"Restore function object: ");
    Js::Var varFunction =  this->Restore(this->functionOffset, /*isFloat64*/ false, /*isInt32*/ false, layout, functionBody);
    Assert(Js::ScriptFunction::Is(varFunction));

    Js::ScriptFunction* function = Js::ScriptFunction::FromVar(varFunction);
    BAILOUT_VERBOSE_TRACE(functionBody, L"Inlinee: %s [%d.%d] \n", function->GetFunctionBody()->GetDisplayName(), function->GetFunctionBody()->GetSourceContextId(), function->GetFunctionBody()->GetLocalFunctionId());

    inlinedFrame->function = function;
    inlinedFrame->callInfo.InlineeStartOffset = inlineeStartOffset;
    inlinedFrame->callInfo.Count = this->argCount;
    inlinedFrame->MapArgs([=](uint i, Js::Var* varRef) {
        bool isFloat64 = floatArgs.Test(i) != 0;
        bool isInt32 = losslessInt32Args.Test(i) != 0;
        BAILOUT_VERBOSE_TRACE(functionBody, L"Restore argument %d: ", i);

        Js::Var var = this->Restore(this->argOffsets[i], isFloat64, isInt32, layout, functionBody);
#if DBG
        if (!Js::TaggedNumber::Is(var))
        {
            Js::RecyclableObject *const recyclableObject = Js::RecyclableObject::FromVar(var);
            Assert(!ThreadContext::IsOnStack(recyclableObject));
        }
#endif
        *varRef = var;
    });
    inlinedFrame->arguments = nullptr;
    BAILOUT_FLUSH(functionBody);
}

void InlineeFrameRecord::RestoreFrames(Js::FunctionBody* functionBody, InlinedFrameLayout* outerMostFrame, Js::JavascriptCallStackLayout* callstack)
{
    InlineeFrameRecord* innerMostRecord = this;
    class AutoReverse
    {
    public:
        InlineeFrameRecord* record;
        AutoReverse(InlineeFrameRecord* record)
        {
            this->record = record->Reverse();
        }

        ~AutoReverse()
        {
            record->Reverse();
        }
    } autoReverse(innerMostRecord);

    InlineeFrameRecord* currentRecord = autoReverse.record;
    InlinedFrameLayout* currentFrame = outerMostFrame;

    int inlineDepth = 1;

    // Find an inlined frame that needs to be restored.
    while (currentFrame->callInfo.Count != 0)
    {
        inlineDepth++;
        currentFrame = currentFrame->Next();
    }

    // Align the inline depth of the record with the frame that needs to be restored
    while (currentRecord && currentRecord->inlineDepth != inlineDepth)
    {
        currentRecord = currentRecord->parent;
    }

    while (currentRecord)
    {
        currentRecord->Restore(functionBody, currentFrame, callstack);
        currentRecord = currentRecord->parent;
        currentFrame = currentFrame->Next();
    }

    // Terminate the inlined stack
    currentFrame->callInfo.Count = 0;
}

Js::Var InlineeFrameRecord::Restore(int offset, bool isFloat64, bool isInt32, Js::JavascriptCallStackLayout * layout, Js::FunctionBody* functionBody) const
{
    Js::Var value;
    bool boxStackInstance = true;
    double dblValue;
    if (offset >= 0)
    {
        Assert(static_cast<uint>(offset) < constantCount);
        value = this->constants[offset];
        boxStackInstance = false;
    }
    else
    {
        BAILOUT_VERBOSE_TRACE(functionBody, L"Stack offset %10d", offset);
        if (isFloat64)
        {
            dblValue = layout->GetDoubleAtOffset(offset);
            value = Js::JavascriptNumber::New(dblValue, functionBody->GetScriptContext());
            BAILOUT_VERBOSE_TRACE(functionBody, L", value: %f (ToVar: 0x%p)", dblValue, value);
        }
        else if (isInt32)
        {
            value = (Js::Var)layout->GetInt32AtOffset(offset);
        }
        else
        {
            value = layout->GetOffset(offset);
        }
    }

    if (isInt32)
    {
        int32 int32Value = ::Math::PointerCastToIntegralTruncate<int32>(value);
        value = Js::JavascriptNumber::ToVar(int32Value, functionBody->GetScriptContext());
        BAILOUT_VERBOSE_TRACE(functionBody, L", value: %10d (ToVar: 0x%p)", int32Value, value);
    }
    else
    {
        BAILOUT_VERBOSE_TRACE(functionBody, L", value: 0x%p", value);
        if (boxStackInstance)
        {
            Js::Var oldValue = value;
            value = Js::JavascriptOperators::BoxStackInstance(oldValue, functionBody->GetScriptContext(), /* allowStackFunction */ true);

#if ENABLE_DEBUG_CONFIG_OPTIONS
            if (oldValue != value)
            {
                BAILOUT_VERBOSE_TRACE(functionBody, L" (Boxed: 0x%p)", value);
            }
#endif
        }
    }
    BAILOUT_VERBOSE_TRACE(functionBody, L"\n");
    return value;
}

InlineeFrameRecord* InlineeFrameRecord::Reverse()
{
    InlineeFrameRecord * prev = nullptr;
    InlineeFrameRecord * current = this;
    while (current)
    {
        InlineeFrameRecord * next = current->parent;
        current->parent = prev;
        prev = current;
        current = next;
    }
    return prev;
}

#if DBG_DUMP

void InlineeFrameRecord::Dump() const
{
    Output::Print(L"%s [#%u.%u] args:", this->functionBody->GetExternalDisplayName(), this->functionBody->GetSourceContextId(), this->functionBody->GetLocalFunctionId());
    for (uint i = 0; i < argCount; i++)
    {
        DumpOffset(argOffsets[i]);
        if (floatArgs.Test(i))
        {
            Output::Print(L"f ");
        }
        else if (losslessInt32Args.Test(i))
        {
            Output::Print(L"i ");
        }
        Output::Print(L", ");
    }
    this->frameInfo->Dump();

    Output::Print(L"func: ");
    DumpOffset(functionOffset);

    if (this->parent)
    {
        parent->Dump();
    }
}

void InlineeFrameRecord::DumpOffset(int offset) const
{
    if (offset >= 0)
    {
        Output::Print(L"%p ", constants[offset]);
    }
    else
    {
        Output::Print(L"<%d> ", offset);
    }
}

void InlineeFrameInfo::Dump() const
{
    Output::Print(L"func: ");
    if (this->function.type == InlineeFrameInfoValueType_Const)
    {
        Output::Print(L"%p(Var) ", this->function.constValue);
    }
    else if (this->function.type == InlineeFrameInfoValueType_Sym)
    {
        this->function.sym->Dump();
        Output::Print(L" ");
    }

    Output::Print(L"args: ");
    arguments->Map([=](uint i, InlineFrameInfoValue& value)
    {
        if (value.type == InlineeFrameInfoValueType_Const)
        {
            Output::Print(L"%p(Var) ", value.constValue);
        }
        else if (value.type == InlineeFrameInfoValueType_Sym)
        {
            value.sym->Dump();
            Output::Print(L" ");
        }
        Output::Print(L", ");
    });
}
#endif
