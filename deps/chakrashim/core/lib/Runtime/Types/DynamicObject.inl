//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

namespace Js
{
#ifdef RECYCLER_STRESS
    // Only enable RecyclerTrackStress on DynamicObject
    template <class T> bool IsRecyclerTrackStressType() { return false; }
    template <> inline bool IsRecyclerTrackStressType<DynamicObject>() { return true; }
#endif

    template <class T>
    __inline T * DynamicObject::NewObject(Recycler * recycler, DynamicType * type)
    {
        size_t inlineSlotsSize = type->GetTypeHandler()->GetInlineSlotsSize();
        if (inlineSlotsSize)
        {
#ifdef RECYCLER_STRESS
            if (Js::Configuration::Global.flags.RecyclerTrackStress && IsRecyclerTrackStressType<T>())
            {
                return RecyclerNewTrackedLeafPlusZ(recycler, inlineSlotsSize, T, type);
            }
#endif
            return RecyclerNewPlusZ(recycler, inlineSlotsSize, T, type);
        }
        else
        {
#ifdef RECYCLER_STRESS
            if (Js::Configuration::Global.flags.RecyclerTrackStress && IsRecyclerTrackStressType<T>())
            {
                return RecyclerNewTrackedLeaf(recycler, T, type);
            }
#endif
            return RecyclerNew(recycler, T, type);
        }
    }
}
