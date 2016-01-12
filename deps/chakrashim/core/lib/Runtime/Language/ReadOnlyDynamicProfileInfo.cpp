//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"
#include "Language\ReadOnlyDynamicProfileInfo.h"

#if ENABLE_PROFILE_INFO
namespace Js
{
    const LdElemInfo *ReadOnlyDynamicProfileInfo::GetLdElemInfo(FunctionBody *functionBody, ProfileId ldElemId)
    {
        Assert(functionBody);
        Assert(ldElemId < functionBody->GetProfiledLdElemCount());

        // This data is accessed multiple times. Since the original profile data may be changing on the foreground thread,
        // the first time it's accessed it will be copied from the original profile data (if we're jitting in the
        // background).
        if(!ldElemInfo)
        {
            if(backgroundAllocator)
            {
                // Jitting in the background
                LdElemInfo *const info = AnewArray(backgroundAllocator, LdElemInfo, functionBody->GetProfiledLdElemCount());
                js_memcpy_s(
                    info,
                    functionBody->GetProfiledLdElemCount() * sizeof(info[0]),
                    profileInfo->GetLdElemInfo(),
                    functionBody->GetProfiledLdElemCount() * sizeof(info[0]));
                ldElemInfo = info;
            }
            else
            {
                // Jitting in the foreground
                ldElemInfo = profileInfo->GetLdElemInfo();
            }
        }
        return &ldElemInfo[ldElemId];
    }

    const StElemInfo *ReadOnlyDynamicProfileInfo::GetStElemInfo(FunctionBody *functionBody, ProfileId stElemId)
    {
        Assert(functionBody);
        Assert(stElemId < functionBody->GetProfiledStElemCount());

        // This data is accessed multiple times. Since the original profile data may be changing on the foreground thread,
        // the first time it's accessed it will be copied from the original profile data (if we're jitting in the
        // background).
        if(!stElemInfo)
        {
            if(backgroundAllocator)
            {
                // Jitting in the background
                StElemInfo *const info = AnewArray(backgroundAllocator, StElemInfo, functionBody->GetProfiledStElemCount());
                js_memcpy_s(
                    info,
                    functionBody->GetProfiledStElemCount() * sizeof(info[0]),
                    profileInfo->GetStElemInfo(),
                    functionBody->GetProfiledStElemCount() * sizeof(info[0]));
                stElemInfo = info;
            }
            else
            {
                // Jitting in the foreground
                stElemInfo = profileInfo->GetStElemInfo();
            }
        }
        return &stElemInfo[stElemId];
    }
}
#endif
