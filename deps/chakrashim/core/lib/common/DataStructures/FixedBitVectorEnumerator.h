//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    class FBVEnumerator
    {

    // Data
    private:
        BVUnit          *icur, *iend;
        BVIndex         curOffset;
        BVUnit          curUnit;

    // Constructor
    public:
        FBVEnumerator(BVUnit * iterStart, BVUnit * iterEnd);

    // Implementation
    protected:
        void MoveToValidWord();
        void MoveToNextBit();

    // Methods
    public:

        void operator++(int);
        BVIndex GetCurrent() const;
        bool End() const;
    };
}
