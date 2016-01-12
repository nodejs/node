//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function func3()
{
    var a = 3;
    // Bailout point #1: test const prop
    return a;
}

WScript.Echo(func3());
