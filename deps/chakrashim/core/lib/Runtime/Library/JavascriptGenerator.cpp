//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "Language\InterpreterStackFrame.h"

namespace Js
{
    JavascriptGenerator::JavascriptGenerator(DynamicType* type, Arguments &args, ScriptFunction* scriptFunction)
        : DynamicObject(type), frame(nullptr), state(GeneratorState::Suspended), args(args), scriptFunction(scriptFunction)
    {
    }

    bool JavascriptGenerator::Is(Var var)
    {
        return JavascriptOperators::GetTypeId(var) == TypeIds_Generator;
    }

    JavascriptGenerator* JavascriptGenerator::FromVar(Var var)
    {
        AssertMsg(Is(var), "Ensure var is actually a 'JavascriptGenerator'");

        return static_cast<JavascriptGenerator*>(var);
    }

    Var JavascriptGenerator::CallGenerator(ResumeYieldData* yieldData, const wchar_t* apiNameForErrorMessage)
    {
        ScriptContext* scriptContext = this->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();
        Var result = nullptr;

        if (this->IsExecuting())
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_GeneratorAlreadyExecuting, apiNameForErrorMessage);
        }

        {
            // RAII helper to set the state of the generator to completed if an exception is thrown
            // or if the save state InterpreterStackFrame is never created implying the generator
            // is JITed and returned without ever yielding.
            class GeneratorStateHelper
            {
                JavascriptGenerator* g;
                bool didThrow;
            public:
                GeneratorStateHelper(JavascriptGenerator* g) : g(g), didThrow(true) { g->SetState(GeneratorState::Executing); }
                ~GeneratorStateHelper() { g->SetState(didThrow || g->frame == nullptr ? GeneratorState::Completed : GeneratorState::Suspended); }
                void DidNotThrow() { didThrow = false; }
            } helper(this);

            Var thunkArgs[] = { this, yieldData };
            Arguments arguments(_countof(thunkArgs), thunkArgs);

            try
            {
                result = JavascriptFunction::CallFunction<1>(this->scriptFunction, this->scriptFunction->GetEntryPoint(), arguments);
                helper.DidNotThrow();
            }
            catch (Js::JavascriptExceptionObject* exceptionObj)
            {
                if (!exceptionObj->IsGeneratorReturnException())
                {
                    throw exceptionObj;
                }
                result = exceptionObj->GetThrownObject(nullptr);
            }
        }

        if (this->IsCompleted())
        {
            result = library->CreateIteratorResultObject(result, library->GetTrue());
        }
        else
        {
            int nextOffset = this->frame->GetReader()->GetCurrentOffset();
            int endOffset = this->frame->GetFunctionBody()->GetByteCode()->GetLength();

            if (nextOffset != endOffset - 1)
            {
                result = library->CreateIteratorResultObjectValueFalse(result);
            }
            else
            {
                result = library->CreateIteratorResultObject(result, library->GetTrue());
                this->SetState(GeneratorState::Completed);
            }
        }

        return result;
    }

    Var JavascriptGenerator::EntryNext(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Generator.prototype.next");

        if (!JavascriptGenerator::Is(args[0]))
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, L"Generator.prototype.next", L"Generator");
        }

        JavascriptGenerator* generator = JavascriptGenerator::FromVar(args[0]);
        Var input = args.Info.Count > 1 ? args[1] : library->GetUndefined();

        if (generator->IsCompleted())
        {
            return library->CreateIteratorResultObjectUndefinedTrue();
        }

        ResumeYieldData yieldData(input, nullptr);
        return generator->CallGenerator(&yieldData, L"Generator.prototype.next");
    }

    Var JavascriptGenerator::EntryReturn(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Generator.prototype.return");

        if (!JavascriptGenerator::Is(args[0]))
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, L"Generator.prototype.return", L"Generator");
        }

        JavascriptGenerator* generator = JavascriptGenerator::FromVar(args[0]);
        Var input = args.Info.Count > 1 ? args[1] : library->GetUndefined();

        if (generator->IsSuspendedStart())
        {
            generator->SetState(GeneratorState::Completed);
        }

        if (generator->IsCompleted())
        {
            return library->CreateIteratorResultObject(input, library->GetTrue());
        }

        ResumeYieldData yieldData(input, RecyclerNew(scriptContext->GetRecycler(), GeneratorReturnExceptionObject, input, scriptContext));
        return generator->CallGenerator(&yieldData, L"Generator.prototype.return");
    }

    Var JavascriptGenerator::EntryThrow(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Generator.prototype.throw");

        if (!JavascriptGenerator::Is(args[0]))
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, L"Generator.prototype.throw", L"Generator");
        }

        JavascriptGenerator* generator = JavascriptGenerator::FromVar(args[0]);
        Var input = args.Info.Count > 1 ? args[1] : library->GetUndefined();

        if (generator->IsSuspendedStart())
        {
            generator->SetState(GeneratorState::Completed);
        }

        if (generator->IsCompleted())
        {
            JavascriptExceptionOperators::OP_Throw(input, scriptContext);
        }

        ResumeYieldData yieldData(input, RecyclerNew(scriptContext->GetRecycler(), JavascriptExceptionObject, input, scriptContext, nullptr));
        return generator->CallGenerator(&yieldData, L"Generator.prototype.throw");
    }
}
