//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template <typename StringType>
    inline void JavascriptString::Copy(__out_ecount(bufLen) wchar_t *const buffer, const charcount_t bufLen)
    {
        Assert(buffer);

        charcount_t stringLen = this->GetLength();
        Assert(stringLen == m_charLength);
        if (bufLen < stringLen)
        {
            Throw::InternalError();
        }

        if (IsFinalized())
        {
            CopyHelper(buffer, GetString(), stringLen);
            return;
        }

        // Copy everything except nested string trees into the buffer. Collect nested string trees and the buffer locations
        // where they need to be copied, and copy them at the end. This is done to avoid excessive recursion during the copy.
        StringCopyInfoStack nestedStringTreeCopyInfos(GetScriptContext());
        ((StringType *)this)->StringType::CopyVirtual(buffer, nestedStringTreeCopyInfos, 0);
        FinishCopy(buffer, nestedStringTreeCopyInfos);
    }
} // namespace Js
