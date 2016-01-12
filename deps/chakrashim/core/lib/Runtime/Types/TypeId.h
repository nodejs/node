//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Js
{

#include "EdgeJavascriptTypeId.h"
    // All WinRT dates are regular Javascript dates too
    inline bool IsDateTypeId(TypeId typeId) { return (typeId == TypeIds_Date || typeId == TypeIds_WinRTDate); }
}
