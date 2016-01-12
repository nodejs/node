//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var o = { a: 0 };
    var b = 3;
    for(var i = 0; i < 3; ++i)
        o.a = 0 / (b >>>= 1);
    return o.a;
};
WScript.Echo(test0());
