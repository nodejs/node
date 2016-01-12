//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{

    class DiagNativeStackFrame;
    //
    // Unified stack frame used by debugger (F12, inside VS)
    // -- interpreter or native stack frame.
    //
    class DiagStackFrame
    {
    public:
        virtual ~DiagStackFrame() {}
        virtual JavascriptFunction* GetJavascriptFunction() = 0;
        virtual int GetByteCodeOffset() = 0;
        virtual DWORD_PTR GetStackAddress() = 0;
        virtual Var GetRegValue(RegSlot slotId, bool allowTemp = false) = 0;
        virtual Var GetNonVarRegValue(RegSlot slotId) = 0;
        virtual void SetRegValue(RegSlot slotId, Var value) = 0;
        virtual Var GetArgumentsObject() = 0;
        virtual Var CreateHeapArguments() = 0;

        virtual ScriptContext* GetScriptContext();
        virtual PCWSTR GetDisplayName();
        virtual bool IsInterpreterFrame();
        virtual InterpreterStackFrame* AsInterpreterFrame();
        virtual ArenaAllocator * GetArena();
        virtual FrameDisplay * GetFrameDisplay();
        virtual Var GetScopeObjectFromFrameDisplay(uint index);
        virtual Var GetRootObject();
        virtual Var GetInnerScopeFromRegSlot(RegSlot location);

        bool IsTopFrame();
        ScriptFunction* GetScriptFunction();
        FunctionBody* GetFunction();

    protected:
        DiagStackFrame(int frameIndex);

    private:
        int frameIndex;
    };

    class DiagInterpreterStackFrame : public DiagStackFrame
    {
        InterpreterStackFrame* m_interpreterFrame;

    public:
        DiagInterpreterStackFrame(InterpreterStackFrame* frame, int frameIndex);
        virtual JavascriptFunction* GetJavascriptFunction() override;
        virtual ScriptContext* GetScriptContext() override;
        virtual int GetByteCodeOffset() override;
        virtual DWORD_PTR GetStackAddress() override;
        virtual bool IsInterpreterFrame() override;
        virtual InterpreterStackFrame* AsInterpreterFrame() override;
        virtual Var GetRegValue(RegSlot slotId, bool allowTemp = false) override;
        virtual Var GetNonVarRegValue(RegSlot slotId) override;
        virtual void SetRegValue(RegSlot slotId, Var value) override;
        virtual Var GetArgumentsObject() override;
        virtual Var CreateHeapArguments() override;
        virtual FrameDisplay * GetFrameDisplay() override;
        virtual Var GetInnerScopeFromRegSlot(RegSlot location) override;
    };

#if ENABLE_NATIVE_CODEGEN
    class DiagNativeStackFrame : public DiagStackFrame
    {
        ScriptFunction* m_function;
        int m_byteCodeOffset;
        void* m_stackAddr;
        int32 m_localVarSlotsOffset; // the offset on the native stack frame where the locals are residing.
        int32 m_localVarChangedOffset; // The offset which stores if any locals is changed from the debugger.

        static const int32 InvalidOffset = -1;

    public:
        DiagNativeStackFrame(ScriptFunction* function, int byteCodeOffset, void* stackAddr, void *codeAddr, int frameIndex);
        virtual JavascriptFunction* GetJavascriptFunction() override;
        virtual ScriptContext* GetScriptContext() override;
        virtual int GetByteCodeOffset() override;
        virtual DWORD_PTR GetStackAddress() override;

        virtual Var GetRegValue(RegSlot slotId, bool allowTemp = false) override;
        virtual Var GetNonVarRegValue(RegSlot slotId) override;
        virtual void SetRegValue(RegSlot slotId, Var value) override;
        virtual Var GetArgumentsObject() override;
        virtual Var CreateHeapArguments() override;

    private:
        Var * GetSlotOffsetLocation(RegSlot slotId, bool allowTemp = false);
    };
#endif

    class DiagRuntimeStackFrame : public DiagStackFrame
    {
        JavascriptFunction* m_function;
        PCWSTR m_displayName;
        void* m_stackAddr;

    public:
        DiagRuntimeStackFrame(JavascriptFunction* function, PCWSTR displayName, void* stackAddr, int frameIndex);
        virtual JavascriptFunction* GetJavascriptFunction() override;
        virtual int GetByteCodeOffset() override;
        virtual DWORD_PTR GetStackAddress() override;
        virtual Var GetRegValue(RegSlot slotId, bool allowTemp = false) override;
        virtual Var GetNonVarRegValue(RegSlot slotId) override;
        virtual void SetRegValue(RegSlot slotId, Var value) override;
        virtual Var GetArgumentsObject() override;
        virtual Var CreateHeapArguments() override;

        virtual PCWSTR GetDisplayName() override;
    };
}
