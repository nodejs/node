//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace regex
{
    struct Nothing { };
    template<typename T0, typename T1, typename T2 = Nothing, typename T3 = Nothing>
    class Tuple
    {
        T0 first;
        T1 second;
        T2 third;
        T3 forth;
    public:
        Tuple(T0 first, T1 second)
            : first(first), second(second)
        {
            CompileAssert(sizeof(T2)==sizeof(Nothing));
            CompileAssert(sizeof(T3)==sizeof(Nothing));
        }

        Tuple(T0 first, T1 second, T2 third)
            : first(first), second(second), third(third)
        {
            CompileAssert(sizeof(T3)==sizeof(Nothing));
        }

        T0 First() const
        {
            return first;
        }

        T1 Second() const
        {
            return second;
        }

        T2 Third() const
        {
            CompileAssert(sizeof(T2)!=sizeof(Nothing));
            return third;
        }

        T3 Forth() const
        {
            CompileAssert(sizeof(T3)!=sizeof(Nothing));
            return forth;
        }
    };
}

