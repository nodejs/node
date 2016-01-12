//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(x) {
    var z = 1+x;
    return x ? 0 : 1;
}
test0(1.1);
test0(1.1);
WScript.Echo("PASSED");
