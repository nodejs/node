//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(o) {
    for(var a = 0; a < 2; ++a)
        (+o).toSource;
};
test0({});
test0({});

function test1() {
    var o = { p: 0 };
    var sum = 0;
    for(var i = 0; i < 1; ++i) {
        o.p |= 0;
        for(var j = 0; j < 1; ++j) {
            o.p = 0;
            if(o.p)
                ++sum;
            for(var k = 0; k < 1; ++k)
                sum += o.p;
        }
    }
    return sum;
};
test1();
test1();

WScript.Echo("pass");
