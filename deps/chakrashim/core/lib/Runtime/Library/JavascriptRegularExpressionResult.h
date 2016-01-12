//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class JavascriptRegularExpressionResult
    {
    private:
        static const PropertyIndex InputIndex = 0;
        static const PropertyIndex IndexIndex = 1;

    public:
        // Constructor for storing matches for a global regex (not captures)
        static JavascriptArray* Create(void *const stackAllocationPointer, JavascriptString* input, ScriptContext* const scriptContext);

        // Constructor for storing captures
        static JavascriptArray* Create(void *const stackAllocationPointer, const int numGroups, JavascriptString* input, ScriptContext* const scriptContext);

        static void SetMatch(JavascriptArray* arr, const UnifiedRegex::GroupInfo match);

        // Must be >= 2, but ensure that it will be equal to the total slot count so that the slot array is not created
        static const PropertyIndex InlineSlotCount = HeapConstants::ObjectGranularity / sizeof(Var);

#if DEBUG
        static bool Is(JavascriptArray* arr);
#endif

    private:
        static void InstantiateForceInlinedMembers();
    };
} // namespace Js
