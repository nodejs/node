//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

﻿function deferredWithRegex() {
    return /[\uD800\uDC00\uFFFF]/.test("\uFFFF");
}

if (deferredWithRegex()) {
    WScript.Echo("Pass");
}
