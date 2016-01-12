//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// Option represents a pointer with explicit null semantics. Use sites are
// required to deal with the null possibility.
//----------------------------------------------------------------------------

#pragma once

namespace regex
{
    template<class T>
    class Option
    {
        const T * value;
    public:
        // Info:        Construct an empty option of type T
        Option()
            : value(nullptr)
        { }

        // Info:        Create an empty or non-empty option of type T
        // Parameters:  value - the pointer to hold. May be null.
        Option(const T * value)
            : value(value)
        { }

        // Info:        Get the held value if there is one. Assert otherwise.
        const T * GetValue() const
        {
            Assert(HasValue());
            return value;
        }

        // Info:        Returns true if there is value.
        bool HasValue() const
        {
            return value!=nullptr;
        }

        // Info:        Get the held value if there is one, otherwise call the given function
        //              to produce a value
        // Parameters:  f - function which produces a value of type T
        template<class F>
        const T * GetValueOrDefault(F f) const
        {
            if(value==nullptr)
            {
                return f();
            }
            return value;
        }

        // Info:        Get the held value if there is one, otherwise return the given default value
        // Parameters:  defaultValue - function which produces a value of type T
        const T* GetValueOrDefaultValue(const T * defaultValue) const
        {
            if(value==nullptr)
            {
                return defaultValue;
            }
            return value;
        }

        // Info:        Get the held value if there is one, otherwise return nullptr
        const T* GetValueOrNull() const
        {
            return value;
        }
    };
}
