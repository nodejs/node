//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

"use strict";

var props = Object.getOwnPropertyNames(RegExp);
props.sort();

for (var i = 0, len = props.length; i < len; i++)
{
    var prop = props[i];
    if (prop === 'prototype')
        continue;
    try
    {
        WScript.Echo("Testing: delete RegExp[" + prop + "]");
        var result = delete RegExp[prop];
        if (result === false)
            WScript.Echo("Error: strict delete returned false");
    }
    catch (err)
    {
        if (!err instanceof TypeError)
            WScript.Echo("Error: strict delete threw a non-TypeError: " + err);
    }
}
