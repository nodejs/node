//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function equal(a, b) {
    if (a == b) {
        print("Correct");
    } else {
        print(">> Fail!");
    }
}

function testMul() {
    print("Int32x4 mul");
    var a = SIMD.Int32x4(0xFFFFFFFF, 0xFFFFFFFF, 0x80000000, 0x0);
    var b = SIMD.Int32x4(0x1, 0xFFFFFFFF, 0x80000000, 0xFFFFFFFF);
    var c = SIMD.Int32x4.mul(a, b);
    equal(-1, SIMD.Int32x4.extractLane(c, 0));
    equal(0x1, SIMD.Int32x4.extractLane(c, 1));
    equal(0x0, SIMD.Int32x4.extractLane(c, 2));
    equal(0x0, SIMD.Int32x4.extractLane(c, 3));

    var d = SIMD.Int32x4(4, 3, 2, 1);
    var e = SIMD.Int32x4(10, 20, 30, 40);
    var f = SIMD.Int32x4.mul(d, e);
    equal(40, SIMD.Int32x4.extractLane(f, 0));
    equal(60, SIMD.Int32x4.extractLane(f, 1));
    equal(60, SIMD.Int32x4.extractLane(f, 2));
    equal(40, SIMD.Int32x4.extractLane(f, 3));
}

testMul();
testMul();
testMul();
testMul();
testMul();
testMul();
testMul();
testMul();