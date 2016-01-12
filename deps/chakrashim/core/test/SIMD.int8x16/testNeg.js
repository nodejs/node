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

function testNeg() {
    print("Int8x16 neg");
    var a = SIMD.Int8x16(-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
    var c = SIMD.Int8x16.neg(a);
    equal(16, SIMD.Int8x16.extractLane(c, 0));
    equal(15, SIMD.Int8x16.extractLane(c, 1));
    equal(14, SIMD.Int8x16.extractLane(c, 2));
    equal(13, SIMD.Int8x16.extractLane(c, 3));
    equal(12, SIMD.Int8x16.extractLane(c, 4));
    equal(11, SIMD.Int8x16.extractLane(c, 5));
    equal(10, SIMD.Int8x16.extractLane(c, 6));
    equal(9, SIMD.Int8x16.extractLane(c, 7));
    equal(8, SIMD.Int8x16.extractLane(c, 8));
    equal(7, SIMD.Int8x16.extractLane(c, 9));
    equal(6, SIMD.Int8x16.extractLane(c, 10));
    equal(5, SIMD.Int8x16.extractLane(c, 11));
    equal(4, SIMD.Int8x16.extractLane(c, 12));
    equal(3, SIMD.Int8x16.extractLane(c, 13));
    equal(2, SIMD.Int8x16.extractLane(c, 14));
    equal(1, SIMD.Int8x16.extractLane(c, 15));

    c = SIMD.Int8x16.neg(SIMD.Int8x16(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1));
    equal(-16, SIMD.Int8x16.extractLane(c, 0));
    equal(-15, SIMD.Int8x16.extractLane(c, 1));
    equal(-14, SIMD.Int8x16.extractLane(c, 2));
    equal(-13, SIMD.Int8x16.extractLane(c, 3));
    equal(-12, SIMD.Int8x16.extractLane(c, 4));
    equal(-11, SIMD.Int8x16.extractLane(c, 5));
    equal(-10, SIMD.Int8x16.extractLane(c, 6));
    equal(-9, SIMD.Int8x16.extractLane(c, 7));
    equal(-8, SIMD.Int8x16.extractLane(c, 8));
    equal(-7, SIMD.Int8x16.extractLane(c, 9));
    equal(-6, SIMD.Int8x16.extractLane(c, 10));
    equal(-5, SIMD.Int8x16.extractLane(c, 11));
    equal(-4, SIMD.Int8x16.extractLane(c, 12));
    equal(-3, SIMD.Int8x16.extractLane(c, 13));
    equal(-2, SIMD.Int8x16.extractLane(c, 14));
    equal(-1, SIMD.Int8x16.extractLane(c, 15));

    var m = SIMD.Int8x16(1, 2, 4, 8, 16, 32, 64, 128, -1, -2, -4, -8, -16, -32, -64, -127);
    var n = SIMD.Int8x16(-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12, -13, -14, -15, -16);
    m = SIMD.Int8x16.neg(m);
    n = SIMD.Int8x16.neg(n);
    equal(-1, SIMD.Int8x16.extractLane(m, 0));
    equal(-2, SIMD.Int8x16.extractLane(m, 1));
    equal(-4, SIMD.Int8x16.extractLane(m, 2));
    equal(-8, SIMD.Int8x16.extractLane(m, 3));
    equal(-16, SIMD.Int8x16.extractLane(m, 4));
    equal(-32, SIMD.Int8x16.extractLane(m, 5));
    equal(-64, SIMD.Int8x16.extractLane(m, 6));
    equal(-128, SIMD.Int8x16.extractLane(m, 7));
    equal(1, SIMD.Int8x16.extractLane(m, 8));
    equal(2, SIMD.Int8x16.extractLane(m, 9));
    equal(4, SIMD.Int8x16.extractLane(m, 10));
    equal(8, SIMD.Int8x16.extractLane(m, 11));
    equal(16, SIMD.Int8x16.extractLane(m, 12));
    equal(32, SIMD.Int8x16.extractLane(m, 13));
    equal(64, SIMD.Int8x16.extractLane(m, 14));
    equal(127, SIMD.Int8x16.extractLane(m, 15));

    equal(1, SIMD.Int8x16.extractLane(n, 0));
    equal(2, SIMD.Int8x16.extractLane(n, 1));
    equal(3, SIMD.Int8x16.extractLane(n, 2));
    equal(4, SIMD.Int8x16.extractLane(n, 3));
    equal(5, SIMD.Int8x16.extractLane(n, 4));
    equal(6, SIMD.Int8x16.extractLane(n, 5));
    equal(7, SIMD.Int8x16.extractLane(n, 6));
    equal(8, SIMD.Int8x16.extractLane(n, 7));
    equal(9, SIMD.Int8x16.extractLane(n, 8));
    equal(10, SIMD.Int8x16.extractLane(n, 9));
    equal(11, SIMD.Int8x16.extractLane(n, 10));
    equal(12, SIMD.Int8x16.extractLane(n, 11));
    equal(13, SIMD.Int8x16.extractLane(n, 12));
    equal(14, SIMD.Int8x16.extractLane(n, 13));
    equal(15, SIMD.Int8x16.extractLane(n, 14));
    equal(16, SIMD.Int8x16.extractLane(n, 15));

}

testNeg();
testNeg();
testNeg();
testNeg();
testNeg();
testNeg();
testNeg();
testNeg();