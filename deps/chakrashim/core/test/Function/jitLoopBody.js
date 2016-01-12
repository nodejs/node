//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var o = { p: 1, q: 2 };
    var sum = 0;
    for(var i = 0; i < 1; ++i)
        if(i !== 0)
            for(var j in o)
                sum += o[j];
    return sum;
}
WScript.Echo("test0: " + test0());
