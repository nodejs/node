//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Test to ensure we honor the attributes of newly added property during enumeration

var a = [ 0, 1, 2 ];
Object.defineProperty(a, "10", { writable: false, value:10, enumerable:true });

var mod = false;
for (var i in a)
{
    if (!mod) {
        Object.defineProperty(a, "5", { writable: false, value:5, enumerable:false });
        mod = true;
    }
    WScript.Echo(i);
}
