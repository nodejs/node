//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    // Helper struct used to communicate to a yield point whether it was resumed via next(), return(), or throw()
    // and provide the data necessary for the corresponding action taken (see OP_ResumeYield)
    struct ResumeYieldData
    {
        Var data;
        JavascriptExceptionObject* exceptionObj;

        ResumeYieldData(Var data, JavascriptExceptionObject* exceptionObj) : data(data), exceptionObj(exceptionObj) { }
    };

    class JavascriptGenerator : public DynamicObject
    {
    public:
        enum class GeneratorState {
            Suspended,
            Executing,
            Completed
        };

        static uint32 GetFrameOffset() { return offsetof(JavascriptGenerator, frame); }
        static uint32 GetCallInfoOffset() { return offsetof(JavascriptGenerator, args) + Arguments::GetCallInfoOffset(); }
        static uint32 GetArgsPtrOffset() { return offsetof(JavascriptGenerator, args) + Arguments::GetValuesOffset(); }

    private:
        InterpreterStackFrame* frame;
        GeneratorState state;
        Arguments args;
        ScriptFunction* scriptFunction;

        DEFINE_VTABLE_CTOR_MEMBER_INIT(JavascriptGenerator, DynamicObject, args);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptGenerator);

        void SetState(GeneratorState state)
        {
            this->state = state;
            if (state == GeneratorState::Completed)
            {
                frame = nullptr;
                args.Values = nullptr;
                scriptFunction = nullptr;
            }
        }

        Var CallGenerator(ResumeYieldData* yieldData, const wchar_t* apiNameForErrorMessage);

    public:
        JavascriptGenerator(DynamicType* type, Arguments& args, ScriptFunction* scriptFunction);

        bool IsExecuting() const { return state == GeneratorState::Executing; }
        bool IsSuspended() const { return state == GeneratorState::Suspended; }
        bool IsCompleted() const { return state == GeneratorState::Completed; }
        bool IsSuspendedStart() const { return state == GeneratorState::Suspended && this->frame == nullptr; }

        void SetFrame(InterpreterStackFrame* frame) { Assert(this->frame == nullptr); this->frame = frame; }
        InterpreterStackFrame* GetFrame() const { return frame; }

        const Arguments& GetArguments() const { return args; }

        static bool Is(Var var);
        static JavascriptGenerator* FromVar(Var var);

        class EntryInfo
        {
        public:
            static FunctionInfo Next;
            static FunctionInfo Return;
            static FunctionInfo Throw;
        };
        static Var EntryNext(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryReturn(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryThrow(RecyclableObject* function, CallInfo callInfo, ...);
    };
}
