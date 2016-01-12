//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"
#include "Types\SpreadArgument.h"
namespace Js
{
    bool SpreadArgument::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_SpreadArgument;
    }
    SpreadArgument* SpreadArgument::FromVar(Var aValue)
    {
        Assert(SpreadArgument::Is(aValue));
        return static_cast<SpreadArgument*>(aValue);
    }

    SpreadArgument::SpreadArgument(Var iterable, RecyclableObject* iterator, DynamicType * type) : DynamicObject(type), iterable(iterable),
        iterator(iterator), iteratorIndices(nullptr)
    {
        Var nextItem;
        ScriptContext * scriptContext = this->GetScriptContext();

        while (JavascriptOperators::IteratorStepAndValue(iterator, scriptContext, &nextItem))
        {
            if (iteratorIndices == nullptr)
            {
                iteratorIndices = RecyclerNew(scriptContext->GetRecycler(), VarList, scriptContext->GetRecycler());
            }

            iteratorIndices->Add(nextItem);
        }
    }

} // namespace Js
