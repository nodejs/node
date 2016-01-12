//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {

    class JavascriptExceptionContext
    {
    public:
        struct StackFrame
        {
        private:
            // Real script frames: functionBody, byteCodeOffset
            // Native library builtin (or potentially virtual) frames: name
            FunctionBody* functionBody;
            union
            {
                uint32 byteCodeOffset;  // used for script functions        (functionBody != nullptr)
                PCWSTR name;            // used for native/virtual frames   (functionBody == nullptr)
            };
            StackTraceArguments argumentTypes;

        public:
            StackFrame() {}
            StackFrame(JavascriptFunction* func, const JavascriptStackWalker& walker, bool initArgumentTypes);

            bool IsScriptFunction() const;
            FunctionBody* GetFunctionBody() const;
            uint32 GetByteCodeOffset() const { return byteCodeOffset; }
            LPCWSTR GetFunctionName() const;
            HRESULT GetFunctionNameWithArguments(_In_ LPCWSTR *outResult) const;
        };

        typedef JsUtil::List<StackFrame> StackTrace;

    public:
        JavascriptExceptionContext() :
            m_throwingFunction(nullptr),
            m_throwingFunctionByteCodeOffset(0),
            m_stackTrace(nullptr),
            m_originalStackTrace(nullptr)
        {
        }

        JavascriptFunction* ThrowingFunction() const { return m_throwingFunction; }
        uint32 ThrowingFunctionByteCodeOffset() const { return m_throwingFunctionByteCodeOffset; }
        void SetThrowingFunction(JavascriptFunction* function, uint32 byteCodeOffset, void * returnAddress);

        bool HasStackTrace() const { return m_stackTrace && m_stackTrace->Count() > 0; }
        StackTrace* GetStackTrace() const { return m_stackTrace; }
        void SetStackTrace(StackTrace *stackTrace) { m_stackTrace = stackTrace; }
        void SetOriginalStackTrace(StackTrace *stackTrace) { Assert(m_originalStackTrace == nullptr); m_originalStackTrace = stackTrace; }
        StackTrace* GetOriginalStackTrace() const { return m_originalStackTrace; }

    private:
        JavascriptFunction* m_throwingFunction;
        uint32 m_throwingFunctionByteCodeOffset;
        StackTrace *m_stackTrace;
        StackTrace *m_originalStackTrace;
    };
}
