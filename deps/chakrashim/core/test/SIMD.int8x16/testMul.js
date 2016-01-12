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
    print("Int8x16 mul");
    var a = SIMD.Int8x16(0xFF, 0xFF, 0x80, 0x0, 0xFF, 0xFF, 0x80, 0x0, 0xFF, 0xFF, 0x80, 0x0, 0xFF, 0xFF, 0x80, 0x0);
    var b = SIMD.Int8x16(0x1, 0xFF, 0x80, 0xFF, 0x1, 0xFF, 0x80, 0xFF, 0x1, 0xFF, 0x80, 0xFF, 0x1, 0xFF, 0x80, 0xFF);
    var c = SIMD.Int8x16.mul(a, b);
    equal(-1, SIMD.Int8x16.extractLane(c, 0));
    equal(0x1, SIMD.Int8x16.extractLane(c, 1));
    equal(0x0, SIMD.Int8x16.extractLane(c, 2));
    equal(0x0, SIMD.Int8x16.extractLane(c, 3));
    equal(-1, SIMD.Int8x16.extractLane(c, 4));
    equal(0x1, SIMD.Int8x16.extractLane(c, 5));
    equal(0x0, SIMD.Int8x16.extractLane(c, 6));
    equal(0x0, SIMD.Int8x16.extractLane(c, 7));
    equal(-1, SIMD.Int8x16.extractLane(c, 8));
    equal(0x1, SIMD.Int8x16.extractLane(c, 9));
    equal(0x0, SIMD.Int8x16.extractLane(c, 10));
    equal(0x0, SIMD.Int8x16.extractLane(c, 11));
    equal(-1, SIMD.Int8x16.extractLane(c, 12));
    equal(0x1, SIMD.Int8x16.extractLane(c, 13));
    equal(0x0, SIMD.Int8x16.extractLane(c, 14));
    equal(0x0, SIMD.Int8x16.extractLane(c, 15));


    var d = SIMD.Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    var e = SIMD.Int8x16(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
    var f = SIMD.Int8x16.mul(d, e);
    equal(16, SIMD.Int8x16.extractLane(f, 0));
    equal(30, SIMD.Int8x16.extractLane(f, 1));
    equal(42, SIMD.Int8x16.extractLane(f, 2));
    equal(52, SIMD.Int8x16.extractLane(f, 3));
    equal(60, SIMD.Int8x16.extractLane(f, 4));
    equal(66, SIMD.Int8x16.extractLane(f, 5));
    equal(70, SIMD.Int8x16.extractLane(f, 6));
    equal(72, SIMD.Int8x16.extractLane(f, 7));
    equal(72, SIMD.Int8x16.extractLane(f, 8));
    equal(70, SIMD.Int8x16.extractLane(f, 9));
    equal(66, SIMD.Int8x16.extractLane(f, 10));
    equal(60, SIMD.Int8x16.extractLane(f, 11));
    equal(52, SIMD.Int8x16.extractLane(f, 12));
    equal(42, SIMD.Int8x16.extractLane(f, 13));
    equal(30, SIMD.Int8x16.extractLane(f, 14));
    equal(16, SIMD.Int8x16.extractLane(f, 15));
}

testMul();
testMul();
testMul();
testMul();
testMul();
testMul();
testMul();
testMul();