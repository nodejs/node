//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    struct VarArrayVarCount
    {
        Var count;
        Var elements[];

        VarArrayVarCount(Var count) : count(count)
        {
        }

        void SetCount(uint count);
        size_t GetDataSize() const;
    };
};
