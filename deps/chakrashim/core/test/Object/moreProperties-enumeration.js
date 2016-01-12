//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Bug 5397: Transition from PropertyIndex to BigPropertyIndex during enumeration causes a crash
var o = {};
var i;
for(i = 0; i < 65530; ++i)
    o["p" + i] = 0;
var add;
for(; i < 65540; ++i) {
    add = true;
    for(var p in o) {
        if(add) {
            add = false;
            WScript.Echo(i);
            o["p" + i] = 0;
        }
    }
}
