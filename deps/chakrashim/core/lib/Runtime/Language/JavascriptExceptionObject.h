//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {

    const DWORD ExceptionCode = ('jsc' | 0xE0000000);

    // As magic numbers increase, we have to keep track of the versions that we are
    // backwards compatible with.
    // Old CRTs also recognize unknown magic numbers with a >= test.  Therefore, we just increment the
    // the magic number by one every time we need another.
    //

    const DWORD  ExceptionParameters = 1;
    const int    ExceptionObjectIndex = 0;

    class JavascriptExceptionContext;

    class JavascriptExceptionObject
    {
    public:
        typedef Var (__stdcall *HostWrapperCreateFuncType)(Var var, ScriptContext * sourceScriptContext, ScriptContext * destScriptContext);

        JavascriptExceptionObject(Var object, ScriptContext * scriptContext, JavascriptExceptionContext* exceptionContextIn, bool isPendingExceptionObject = false) :
            thrownObject(object), isPendingExceptionObject(isPendingExceptionObject),
            scriptContext(scriptContext), tag(true), isDebuggerSkip(false), byteCodeOffsetAfterDebuggerSkip(Constants::InvalidByteCodeOffset), hasDebuggerLogged(false),
            isFirstChance(false), isExceptionCaughtInNonUserCode(false), ignoreAdvanceToNextStatement(false), hostWrapperCreateFunc(nullptr), isGeneratorReturnException(false)
        {
            if (exceptionContextIn)
            {
                exceptionContext = *exceptionContextIn;
            }
            else
            {
                memset(&exceptionContext, 0, sizeof(exceptionContext));
            }
#if DBG
            this->stackBackTrace = nullptr;
#endif
        }

        Var GetThrownObject(ScriptContext * requestingScriptContext);

        // ScriptContext can be NULL in case of OOM exception.
        ScriptContext * JavascriptExceptionObject::GetScriptContext() const
        {
            return scriptContext;
        }

        FunctionBody * GetFunctionBody() const;
        JavascriptFunction* GetFunction() const
        {
            return exceptionContext.ThrowingFunction();
        }

        const JavascriptExceptionContext* GetExceptionContext() const
        {
            return &exceptionContext;
        }
#if DBG
        void FillStackBackTrace();
#endif

        void FillError(JavascriptExceptionContext& exceptionContext, ScriptContext *scriptContext, HostWrapperCreateFuncType hostWrapperCreateFunc = NULL);
        void ClearError();

        void SetDebuggerSkip(bool skip)
        {
            isDebuggerSkip = skip;
        }

        bool IsDebuggerSkip()
        {
            return isDebuggerSkip;
        }

        int GetByteCodeOffsetAfterDebuggerSkip()
        {
            return byteCodeOffsetAfterDebuggerSkip;
        }

        void SetByteCodeOffsetAfterDebuggerSkip(int offset)
        {
            byteCodeOffsetAfterDebuggerSkip = offset;
        }

        void SetDebuggerHasLogged(bool has)
        {
            hasDebuggerLogged = has;
        }

        bool HasDebuggerLogged()
        {
            return hasDebuggerLogged;
        }

        void SetIsFirstChance(bool is)
        {
            isFirstChance = is;
        }

        bool IsFirstChanceException()
        {
            return isFirstChance;
        }
        void SetIsExceptionCaughtInNonUserCode(bool is)
        {
            isExceptionCaughtInNonUserCode = is;
        }

        bool IsExceptionCaughtInNonUserCode()
        {
            return isExceptionCaughtInNonUserCode;
        }

        void SetHostWrapperCreateFunc(HostWrapperCreateFuncType hostWrapperCreateFunc)
        {
            this->hostWrapperCreateFunc = hostWrapperCreateFunc;
        }

        uint32 GetByteCodeOffset()
        {
            return exceptionContext.ThrowingFunctionByteCodeOffset();
        }

        void ReplaceThrownObject(Var object)
        {
            AssertMsg(RecyclableObject::Is(object), "Why are we replacing a non recyclable thrown object?");
            AssertMsg(this->GetScriptContext() != RecyclableObject::FromVar(object)->GetScriptContext(), "If replaced thrownObject is from same context what's the need to replace?");
            this->thrownObject = object;
        }

        void SetThrownObject(Var object)
        {
            // Only pending exception object and generator return exception use this API.
            Assert(this->isPendingExceptionObject || this->isGeneratorReturnException);
            this->thrownObject = object;
        }
        JavascriptExceptionObject* JavascriptExceptionObject::CloneIfStaticExceptionObject(ScriptContext* scriptContext);

        void ClearStackTrace()
        {
            exceptionContext.SetStackTrace(NULL);
        }

        bool IsPendingExceptionObject() const { return isPendingExceptionObject; }

        void SetIgnoreAdvanceToNextStatement(bool is)
        {
            ignoreAdvanceToNextStatement = is;
        }

        bool IsIgnoreAdvanceToNextStatement()
        {
            return ignoreAdvanceToNextStatement;
        }

        void SetGeneratorReturnException(bool is)
        {
            isGeneratorReturnException = is;
        }

        bool IsGeneratorReturnException()
        {
            // Used by the generators to throw an exception to indicate the return from generator function
            return isGeneratorReturnException;
        }

    private:
        Var      thrownObject;
        ScriptContext * scriptContext;

        int        byteCodeOffsetAfterDebuggerSkip;
        const bool tag : 1;               // Tag the low bit to prevent possible GC false references
        bool       isPendingExceptionObject : 1;
        bool       isGeneratorReturnException : 1;

        bool       isDebuggerSkip : 1;
        bool       hasDebuggerLogged : 1;
        bool       isFirstChance : 1;      // Mentions whether the current exception is a handled exception or not
        bool       isExceptionCaughtInNonUserCode : 1; // Mentions if in the caller chain the exception will be handled by the non-user code.
        bool       ignoreAdvanceToNextStatement : 1;  // This will be set when user had setnext while sitting on the exception
                                                // So the exception eating logic shouldn't try and advance to next statement again.

        HostWrapperCreateFuncType hostWrapperCreateFunc;

        JavascriptExceptionContext exceptionContext;
#if DBG
        StackBackTrace * stackBackTrace;
        static const int StackToSkip = 2;
        static const int StackTraceDepth = 30;
#endif
    };

    class GeneratorReturnExceptionObject : public JavascriptExceptionObject
    {
    public:
        GeneratorReturnExceptionObject(Var object, ScriptContext * scriptContext)
            : JavascriptExceptionObject(object, scriptContext, nullptr)
        {
            this->SetDebuggerSkip(true);
            this->SetIgnoreAdvanceToNextStatement(true);
            this->SetGeneratorReturnException(true);
        }
    };
}
