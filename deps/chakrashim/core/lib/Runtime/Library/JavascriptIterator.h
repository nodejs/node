//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class JavascriptIterator
    {
    public:
        class EntryInfo
        {
        public:
            static FunctionInfo SymbolIterator;
        };

        static Var EntrySymbolIterator(RecyclableObject* function, CallInfo callInfo, ...);
    };
} // namespace Js
