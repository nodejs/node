//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
class ForcedMemoryConstraint
{
public:
    static void Apply();

private:
    static void FragmentAddressSpace(size_t usableSize);
};
#endif
}
