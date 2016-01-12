//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    JavascriptEnumerator::JavascriptEnumerator(ScriptContext* scriptContext) : RecyclableObject(scriptContext->GetLibrary()->GetEnumeratorType())
    {
        Assert(scriptContext != NULL);
    }

    bool JavascriptEnumerator::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Enumerator;
    }

    JavascriptEnumerator* JavascriptEnumerator::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptEnumerator'");

        return static_cast<JavascriptEnumerator *>(RecyclableObject::FromVar(aValue));
    }
}
