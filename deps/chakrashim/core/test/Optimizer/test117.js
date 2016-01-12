//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(a) {
    for(var i = 0; i < 1 ; ++i) {
        a[1] = 0;
        a[0] = 0;
    }
}
test0([0, 0]);
var a = [];
test0(a);
WScript.Echo("test0: " + a[1]);
