//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template <typename Key, bool zero>
    struct SameValueComparerCommon
    {
        static bool Equals(Key, Key) { static_assert(false, "Can only use SameValueComparer with Var as the key type"); }
        static hash_t GetHashCode(Key) { static_assert(false, "Can only use SameValueComparer with Var as the key type"); }
    };

    template <typename Key> using SameValueComparer = SameValueComparerCommon<Key, false>;
    template <typename Key> using SameValueZeroComparer = SameValueComparerCommon<Key, true>;

    template <bool zero>
    struct SameValueComparerCommon<Var, zero>
    {
        static bool Equals(Var x, Var y)
        {
            if (zero)
            {
                return JavascriptConversion::SameValueZero(x, y);
            }
            else
            {
                return JavascriptConversion::SameValue(x, y);
            }
        }

        static hash_t GetHashCode(Var i)
        {
            switch (JavascriptOperators::GetTypeId(i))
            {
            case TypeIds_Integer:
                return TaggedInt::ToInt32(i);

            case TypeIds_Int64Number:
            case TypeIds_UInt64Number:
                {
                    __int64 v = JavascriptInt64Number::FromVar(i)->GetValue();
                    return (uint)v ^ (uint)(v >> 32);
                }

            case TypeIds_Number:
                {
                    double d = JavascriptNumber::GetValue(i);
                    if (JavascriptNumber::IsNan(d))
                    {
                        return 0;
                    }

                    if (zero)
                    {
                        // SameValueZero treats -0 and +0 the same, so normalize to get same hash code
                        if (JavascriptNumber::IsNegZero(d))
                        {
                            d = 0.0;
                        }
                    }

                    __int64 v = *(__int64*)&d;
                    return (uint)v ^ (uint)(v >> 32);
                }

            case TypeIds_String:
                {
                    JavascriptString* v = JavascriptString::FromVar(i);
                    return JsUtil::CharacterBuffer<WCHAR>::StaticGetHashCode(v->GetString(), v->GetLength());
                }

            default:
                return RecyclerPointerComparer<Var>::GetHashCode(i);
            }
        }
    };
}
