//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    __inline JavascriptArray* JavascriptRegularExpressionResult::Create(
        void *const stackAllocationPointer,
        JavascriptString* input,
        ScriptContext* const scriptContext)
    {
        JavascriptArray* arr = JavascriptArray::New<JavascriptArray, InlineSlotCount>(stackAllocationPointer, 0, scriptContext->GetLibrary()->GetRegexResultType()); // use default array capacity

        Assert(JavascriptRegularExpressionResult::Is(arr));
        arr->SetSlot(SetSlotArguments(BuiltInPropertyRecords::input.propertyRecord.GetPropertyId(), InputIndex, input));

        // SetMatch must be called with a valid match before the object is given to script code
        // Each item will be filled in by the match loop
        return arr;
    }

    __inline JavascriptArray* JavascriptRegularExpressionResult::Create(
        void *const stackAllocationPointer,
        const int numGroups,
        JavascriptString* input,
        ScriptContext* const scriptContext)
    {
        Assert(numGroups > 0);

        JavascriptArray* arr = JavascriptArray::NewLiteral<JavascriptArray, InlineSlotCount>(stackAllocationPointer, numGroups, scriptContext->GetLibrary()->GetRegexResultType());

        Assert(JavascriptRegularExpressionResult::Is(arr));
        arr->SetSlot(SetSlotArguments(BuiltInPropertyRecords::input.propertyRecord.GetPropertyId(), InputIndex, input));

        // SetMatch must be called with a valid match before the object is given to script code
        // Each item will be filled in by the match loop
        return arr;
    }

#if DEBUG
    //
    // Check if a given JavascriptArray is actually JavascriptRegularExpressionResult.
    //
    bool JavascriptRegularExpressionResult::Is(JavascriptArray* arr)
    {
        return arr->GetPropertyIndex(PropertyIds::input) == InputIndex
            && arr->GetPropertyIndex(PropertyIds::index) == IndexIndex;
    }
#endif

    void JavascriptRegularExpressionResult::SetMatch(JavascriptArray* arr, const UnifiedRegex::GroupInfo match)
    {
        Assert(JavascriptRegularExpressionResult::Is(arr));
        Assert(!match.IsUndefined());

        ScriptContext* scriptContext = arr->GetScriptContext();
        arr->SetSlot(SetSlotArguments(BuiltInPropertyRecords::index.propertyRecord.GetPropertyId(), IndexIndex, JavascriptNumber::ToVar(match.offset, scriptContext)));
    }

    void JavascriptRegularExpressionResult::InstantiateForceInlinedMembers()
    {
        // Force-inlined functions defined in a translation unit need a reference from an extern non-force-inlined function in
        // the same translation unit to force an instantiation of the force-inlined function. Otherwise, if the force-inlined
        // function is not referenced in the same translation unit, it will not be generated and the linker is not able to find
        // the definition to inline the function in other translation units.
        Assert(false);

        Create(nullptr, nullptr, nullptr);
        Create(nullptr, 0, nullptr, nullptr);
    }

    CompileAssert(JavascriptRegularExpressionResult::InlineSlotCount >= 2);
}
