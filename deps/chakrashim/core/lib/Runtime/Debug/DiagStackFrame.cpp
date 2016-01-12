//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"
#include "Language\JavascriptFunctionArgIndex.h"
#include "Language\InterpreterStackFrame.h"

namespace Js
{
    DiagStackFrame::DiagStackFrame(int frameIndex) :
        frameIndex(frameIndex)
    {
        Assert(frameIndex >= 0);
    }

    // Returns whether or not this frame is on the top of the callstack.
    bool DiagStackFrame::IsTopFrame()
    {
        return this->frameIndex == 0 && GetScriptContext()->GetDebugContext()->GetProbeContainer()->IsPrimaryBrokenToDebuggerContext();
    }

    ScriptFunction* DiagStackFrame::GetScriptFunction()
    {
        return ScriptFunction::FromVar(GetJavascriptFunction());
    }

    FunctionBody* DiagStackFrame::GetFunction()
    {
        return GetJavascriptFunction()->GetFunctionBody();
    }

    ScriptContext* DiagStackFrame::GetScriptContext()
    {
        return GetJavascriptFunction()->GetScriptContext();
    }

    PCWSTR DiagStackFrame::GetDisplayName()
    {
        return GetFunction()->GetExternalDisplayName();
    }

    bool DiagStackFrame::IsInterpreterFrame()
    {
        return false;
    }

    InterpreterStackFrame* DiagStackFrame::AsInterpreterFrame()
    {
        AssertMsg(FALSE, "AsInterpreterFrame called for non-interpreter frame.");
        return nullptr;
    }

    ArenaAllocator * DiagStackFrame::GetArena()
    {
        Assert(GetScriptContext() != NULL);
        return GetScriptContext()->GetThreadContext()->GetDebugManager()->GetDiagnosticArena()->Arena();
    }

    FrameDisplay * DiagStackFrame::GetFrameDisplay()
    {
        FrameDisplay *display = NULL;

        Assert(this->GetFunction() != NULL);
        RegSlot frameDisplayReg = this->GetFunction()->GetFrameDisplayRegister();

        if (frameDisplayReg != Js::Constants::NoRegister && frameDisplayReg != 0)
        {
            display = (FrameDisplay*)this->GetNonVarRegValue(frameDisplayReg);
        }
        else
        {
            display = this->GetScriptFunction()->GetEnvironment();
        }
 
        return display;
    }

    Var DiagStackFrame::GetScopeObjectFromFrameDisplay(uint index)
    {
        FrameDisplay * display = GetFrameDisplay();
        return (display != NULL && display->GetLength() > index) ? display->GetItem(index) : NULL;
    }

    Var DiagStackFrame::GetRootObject()
    {
        Assert(this->GetFunction());
        return this->GetFunction()->LoadRootObject();
    }

    Var DiagStackFrame::GetInnerScopeFromRegSlot(RegSlot location)
    {
        return GetNonVarRegValue(location);
    }

    DiagInterpreterStackFrame::DiagInterpreterStackFrame(InterpreterStackFrame* frame, int frameIndex) :
        DiagStackFrame(frameIndex),
        m_interpreterFrame(frame)
    {
        Assert(m_interpreterFrame != NULL);
        AssertMsg(m_interpreterFrame->GetScriptContext() && m_interpreterFrame->GetScriptContext()->IsInDebugMode(),
            "This only supports interpreter stack frames running in debug mode.");
    }

    JavascriptFunction* DiagInterpreterStackFrame::GetJavascriptFunction()
    {
        return m_interpreterFrame->GetJavascriptFunction();
    }

    ScriptContext* DiagInterpreterStackFrame::GetScriptContext()
    {
        return m_interpreterFrame->GetScriptContext();
    }

    int DiagInterpreterStackFrame::GetByteCodeOffset()
    {
        return m_interpreterFrame->GetReader()->GetCurrentOffset();
    }

    // Address on stack that belongs to current frame.
    // Currently we only use this to determine which of given frames is above/below another one.
    DWORD_PTR DiagInterpreterStackFrame::GetStackAddress()
    {
        return m_interpreterFrame->GetStackAddress();
    }

    bool DiagInterpreterStackFrame::IsInterpreterFrame()
    {
        return true;
    }

    InterpreterStackFrame* DiagInterpreterStackFrame::AsInterpreterFrame()
    {
        return m_interpreterFrame;
    }

    Var DiagInterpreterStackFrame::GetRegValue(RegSlot slotId, bool allowTemp)
    {
        return m_interpreterFrame->GetReg(slotId);
    }

    Var DiagInterpreterStackFrame::GetNonVarRegValue(RegSlot slotId)
    {
        return m_interpreterFrame->GetNonVarReg(slotId);
    }

    void DiagInterpreterStackFrame::SetRegValue(RegSlot slotId, Var value)
    {
        m_interpreterFrame->SetReg(slotId, value);
    }

    Var DiagInterpreterStackFrame::GetArgumentsObject()
    {
        return m_interpreterFrame->GetArgumentsObject();
    }

    Var DiagInterpreterStackFrame::CreateHeapArguments()
    {
        return m_interpreterFrame->CreateHeapArguments(GetScriptContext());
    }

    FrameDisplay * DiagInterpreterStackFrame::GetFrameDisplay()
    {
        return m_interpreterFrame->GetFrameDisplayForNestedFunc();
    }

    Var DiagInterpreterStackFrame::GetInnerScopeFromRegSlot(RegSlot location)
    {
        return m_interpreterFrame->InnerScopeFromRegSlot(location);
    }

#if ENABLE_NATIVE_CODEGEN
    DiagNativeStackFrame::DiagNativeStackFrame(
        ScriptFunction* function,
        int byteCodeOffset,
        void* stackAddr,
        void *codeAddr,
        int frameIndex) :
        DiagStackFrame(frameIndex),
        m_function(function),
        m_byteCodeOffset(byteCodeOffset),
        m_stackAddr(stackAddr),
        m_localVarSlotsOffset(InvalidOffset),
        m_localVarChangedOffset(InvalidOffset)
    {
        Assert(m_stackAddr != NULL);
        AssertMsg(m_function && m_function->GetScriptContext() && m_function->GetScriptContext()->IsInDebugMode(),
            "This only supports functions in debug mode.");

        FunctionEntryPointInfo * entryPointInfo = GetFunction()->GetEntryPointFromNativeAddress((DWORD_PTR)codeAddr);
        if (entryPointInfo)
        {
            m_localVarSlotsOffset = entryPointInfo->localVarSlotsOffset;
            m_localVarChangedOffset = entryPointInfo->localVarChangedOffset;
        }
        else
        {
            AssertMsg(FALSE, "Failed to get entry point for native address. Most likely the frame is old/gone.");
        }
        OUTPUT_TRACE(Js::DebuggerPhase, L"DiagNativeStackFrame::DiagNativeStackFrame: e.p(addr %p)=%p varOff=%d changedOff=%d\n", codeAddr, entryPointInfo, m_localVarSlotsOffset, m_localVarChangedOffset);
    }

    JavascriptFunction* DiagNativeStackFrame::GetJavascriptFunction()
    {
        return m_function;
    }

    ScriptContext* DiagNativeStackFrame::GetScriptContext()
    {
        return m_function->GetScriptContext();
    }

    int DiagNativeStackFrame::GetByteCodeOffset()
    {
        return m_byteCodeOffset;
    }

    // Address on stack that belongs to current frame.
    // Currently we only use this to determine which of given frames is above/below another one.
    DWORD_PTR DiagNativeStackFrame::GetStackAddress()
    {
        return reinterpret_cast<DWORD_PTR>(m_stackAddr);
    }

    Var DiagNativeStackFrame::GetRegValue(RegSlot slotId, bool allowTemp)
    {
        Js::Var *varPtr = GetSlotOffsetLocation(slotId, allowTemp);
        return (varPtr != NULL) ? *varPtr : NULL;
    }

    Var * DiagNativeStackFrame::GetSlotOffsetLocation(RegSlot slotId, bool allowTemp)
    {
        Assert(GetFunction() != NULL);

        int32 slotOffset;
        if (GetFunction()->GetSlotOffset(slotId, &slotOffset, allowTemp))
        {
            Assert(m_localVarSlotsOffset != InvalidOffset);
            slotOffset = m_localVarSlotsOffset + slotOffset;

            // We will have the var offset only (which is always the Var size. With TypeSpecialization, below will change to accommodate double offset.
            return (Js::Var *)(((char *)m_stackAddr) + slotOffset);
        }

        Assert(false);
        return NULL;
    }

    Var DiagNativeStackFrame::GetNonVarRegValue(RegSlot slotId)
    {
        return GetRegValue(slotId);
    }

    void DiagNativeStackFrame::SetRegValue(RegSlot slotId, Var value)
    {
        Js::Var *varPtr = GetSlotOffsetLocation(slotId);
        Assert(varPtr != NULL);

        // First assign the value
        *varPtr = value;

        Assert(m_localVarChangedOffset != InvalidOffset);

        // Now change the bit in the stack which tells that current stack values got changed.
        char *stackOffset = (((char *)m_stackAddr) + m_localVarChangedOffset);

        Assert(*stackOffset == 0 || *stackOffset == FunctionBody::LocalsChangeDirtyValue);

        *stackOffset = FunctionBody::LocalsChangeDirtyValue;
    }

    Var DiagNativeStackFrame::GetArgumentsObject()
    {
        return (Var)((void **)m_stackAddr)[JavascriptFunctionArgIndex_ArgumentsObject];
    }

    Var DiagNativeStackFrame::CreateHeapArguments()
    {
        // We would be creating the arguments object if there is no default arguments object present.
        Assert(GetArgumentsObject() == NULL);

        CallInfo const * callInfo  = (CallInfo const *)&(((void **)m_stackAddr)[JavascriptFunctionArgIndex_CallInfo]);

        // At the least we will have 'this' by default.
        Assert(callInfo->Count > 0);

        // Get the passed parameter's position (which is starting from 'this')
        Var * inParams = (Var *)&(((void **)m_stackAddr)[JavascriptFunctionArgIndex_This]);

        return JavascriptOperators::LoadHeapArguments(
                                        m_function,
                                        callInfo->Count - 1,
                                        &inParams[1],
                                        GetScriptContext()->GetLibrary()->GetNull(),
                                        (PropertyId*)GetScriptContext()->GetLibrary()->GetNull(),
                                        GetScriptContext(),
                                        /* formalsAreLetDecls */ false);
    }
#endif


    DiagRuntimeStackFrame::DiagRuntimeStackFrame(JavascriptFunction* function, PCWSTR displayName, void* stackAddr, int frameIndex):
        DiagStackFrame(frameIndex),
        m_function(function),
        m_displayName(displayName),
        m_stackAddr(stackAddr)
    {
    }

    JavascriptFunction* DiagRuntimeStackFrame::GetJavascriptFunction()
    {
        return m_function;
    }

    PCWSTR DiagRuntimeStackFrame::GetDisplayName()
    {
        return m_displayName;
    }

    DWORD_PTR DiagRuntimeStackFrame::GetStackAddress()
    {
        return reinterpret_cast<DWORD_PTR>(m_stackAddr);
    }

    int DiagRuntimeStackFrame::GetByteCodeOffset()
    {
        return 0;
    }

    Var DiagRuntimeStackFrame::GetRegValue(RegSlot slotId, bool allowTemp)
    {
        return nullptr;
    }

    Var DiagRuntimeStackFrame::GetNonVarRegValue(RegSlot slotId)
    {
        return nullptr;
    }

    void DiagRuntimeStackFrame::SetRegValue(RegSlot slotId, Var value)
    {
    }

    Var DiagRuntimeStackFrame::GetArgumentsObject()
    {
        return nullptr;
    }

    Var DiagRuntimeStackFrame::CreateHeapArguments()
    {
        return nullptr;
    }

}  // namespace Js
