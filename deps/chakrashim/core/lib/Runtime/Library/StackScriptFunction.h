//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
namespace Js
{
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
#define BOX_PARAM(function, returnAddress, reason) function, returnAddress, reason
#else
#define BOX_PARAM(function, returnAddress, reason) function, returnAddress
#endif
    class StackScriptFunction : public ScriptFunction
    {
    public:

        static JavascriptFunction * EnsureBoxed(BOX_PARAM(JavascriptFunction * function, void * returnAddress, wchar_t const * reason));
        static void Box(FunctionBody * functionBody, ScriptFunction ** functionRef);
        static ScriptFunction * OP_NewStackScFunc(FrameDisplay *environment, FunctionProxy** proxyRef, ScriptFunction * stackFunction);
        static uint32 GetOffsetOfBoxedScriptFunction() { return offsetof(StackScriptFunction, boxedScriptFunction); }

        static JavascriptFunction * GetCurrentFunctionObject(JavascriptFunction * function);

        StackScriptFunction(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType) :
            ScriptFunction(proxy, deferredPrototypeType), boxedScriptFunction(nullptr) {};
#if DBG
        static bool IsBoxed(Var var)
        {
            return StackScriptFunction::FromVar(var)->boxedScriptFunction != nullptr;
        }


#endif
        // A stack function object does not escape, so it should never be marshaled.
        // Defining this function also make the vtable unique, so that we can detect stack function
        // via the vtable
        DEFINE_VTABLE_CTOR(StackScriptFunction, ScriptFunction);
        virtual void MarshalToScriptContext(Js::ScriptContext * scriptContext) override
        {
            Assert(false);
        }
    private:
        static ScriptFunction * Box(StackScriptFunction * stackScriptFunction, void * returnAddress);
        static StackScriptFunction * FromVar(Var var);
        struct BoxState
        {
        public:
            BoxState(ArenaAllocator * alloc, FunctionBody * functionBody, ScriptContext * scriptContext, void * returnAddress = nullptr);
            void Box();
            ScriptFunction * GetBoxedScriptFunction(ScriptFunction * stackScriptFunction);
        private:
            JsUtil::BaseHashSet<FunctionBody *, ArenaAllocator> frameToBox;
            JsUtil::BaseHashSet<FunctionProxy *, ArenaAllocator> functionObjectToBox;
            JsUtil::BaseDictionary<void *, void*, ArenaAllocator> boxedValues;
            ScriptContext * scriptContext;
            void * returnAddress;

            Js::Var * BoxScopeSlots(Js::Var * scopeSlots, uint count);
            bool NeedBoxFrame(FunctionBody * functionBody);
            bool NeedBoxScriptFunction(ScriptFunction * scriptFunction);
            ScriptFunction * BoxStackFunction(StackScriptFunction * scriptFunction);
            FrameDisplay * BoxFrameDisplay(FrameDisplay * frameDisplay);
            void BoxNativeFrame(JavascriptStackWalker const& walker, FunctionBody * callerFunctionBody);
            void UpdateFrameDisplay(ScriptFunction *nestedFunc);
            void Finish();

            template<class Fn>
            void ForEachStackNestedFunction(JavascriptStackWalker const& walker, FunctionBody * callerFunctionBody, Fn fn);
            template<class Fn>
            void ForEachStackNestedFunctionInterpreted(InterpreterStackFrame *interpreterFrame, FunctionBody * callerFunctionBody, Fn fn);
            template<class Fn>
            void ForEachStackNestedFunctionNative(JavascriptStackWalker const& walker, FunctionBody * callerFunctionBody, Fn fn);
        };

        ScriptFunction * boxedScriptFunction;
    };
};
