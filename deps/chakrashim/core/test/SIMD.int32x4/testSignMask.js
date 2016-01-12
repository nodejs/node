//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function equal(a, b) {
    if (a == b)
    {
        print("Correct");
    }
    else
    {
        print(">> Fail!");
    }
}

function testSignMask() {
    print("Int32x4 signmask");
    var a = SIMD.Int32x4(0x80000000, 0x7000000, 0xFFFFFFFF, 0x0);
    equal(0x5, a.signMask);
    var b = SIMD.Int32x4(0x0, 0x0, 0x0, 0x0);
    equal(0x0, b.signMask);
    var c = SIMD.Int32x4(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
    equal(0xf, c.signMask);
}

testSignMask();
testSignMask();
testSignMask();
testSignMask();
testSignMask();
testSignMask();
testSignMask();
testSignMask();