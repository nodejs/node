//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template <class TKey, class TValue> struct KeyValuePair
    {
    private:
        TKey key;
        TValue value;

    public:
        KeyValuePair()
        {
        }

        KeyValuePair(TKey key, TValue value)
        {
            this->key = key;
            this->value = value;
        }

        TKey Key() { return key; }
        const TKey Key() const { return key; }

        TValue Value() { return value; }
        const TValue Value() const { return value; }
    };

}
