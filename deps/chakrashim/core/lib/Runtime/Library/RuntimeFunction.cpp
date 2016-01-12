//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    RuntimeFunction::RuntimeFunction(DynamicType * type)
        : JavascriptFunction(type), functionNameId(nullptr)
    {}

    RuntimeFunction::RuntimeFunction(DynamicType * type, FunctionInfo * functionInfo)
        : JavascriptFunction(type, functionInfo), functionNameId(nullptr)
    {}

    RuntimeFunction::RuntimeFunction(DynamicType * type, FunctionInfo * functionInfo, ConstructorCache* cache)
        : JavascriptFunction(type, functionInfo, cache), functionNameId(nullptr)
    {}

    Var
    RuntimeFunction::EnsureSourceString()
    {
        JavascriptLibrary* library = this->GetLibrary();
        ScriptContext * scriptContext = library->GetScriptContext();
        if (this->functionNameId == nullptr)
        {
            this->functionNameId = library->GetFunctionDisplayString();
        }
        else
        {
            if (TaggedInt::Is(this->functionNameId))
            {
                if (this->GetScriptContext()->GetConfig()->IsES6FunctionNameEnabled() && this->GetTypeHandler()->IsDeferredTypeHandler())
                {
                    JavascriptString* functionName = nullptr;
                    DebugOnly(bool status = ) this->GetFunctionName(&functionName);
                    Assert(status);
                    this->SetPropertyWithAttributes(PropertyIds::name, functionName, PropertyConfigurable, nullptr);
                }
                this->functionNameId = GetNativeFunctionDisplayString(scriptContext, scriptContext->GetPropertyString(TaggedInt::ToInt32(this->functionNameId)));
            }
        }
        Assert(JavascriptString::Is(this->functionNameId));
        return this->functionNameId;
    }

    void
    RuntimeFunction::SetFunctionNameId(Var nameId)
    {
        Assert(functionNameId == NULL);
        Assert(TaggedInt::Is(nameId) || Js::JavascriptString::Is(nameId));

        // We are only reference the propertyId, it needs to be tracked to stay alive
        Assert(!TaggedInt::Is(nameId) || this->GetScriptContext()->IsTrackedPropertyId(TaggedInt::ToInt32(nameId)));
        this->functionNameId = nameId;
    }
};
