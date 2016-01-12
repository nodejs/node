//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class RejitException : public ExceptionBase
    {
    private:
        const RejitReason reason;

    public:
        RejitException(const RejitReason reason) : reason(reason)
        {
        }

    public:
        RejitReason Reason() const
        {
            return reason;
        }

        const char *ReasonName() const
        {
            return RejitReasonNames[reason];
        }
    };
}
