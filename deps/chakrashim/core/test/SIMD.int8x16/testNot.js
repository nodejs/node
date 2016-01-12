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

function testNot() {
    print("Int8x16 not");
    var a = SIMD.Int8x16(-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
    var c = SIMD.Int8x16.not(a);
    equal(15, SIMD.Int8x16.extractLane(c, 0));
    equal(14, SIMD.Int8x16.extractLane(c, 1));
    equal(13, SIMD.Int8x16.extractLane(c, 2));
    equal(12, SIMD.Int8x16.extractLane(c, 3));
    equal(11, SIMD.Int8x16.extractLane(c, 4));
    equal(10, SIMD.Int8x16.extractLane(c, 5));
    equal(9, SIMD.Int8x16.extractLane(c, 6));
    equal(8, SIMD.Int8x16.extractLane(c, 7));
    equal(7, SIMD.Int8x16.extractLane(c, 8));
    equal(6, SIMD.Int8x16.extractLane(c, 9));
    equal(5, SIMD.Int8x16.extractLane(c, 10));
    equal(4, SIMD.Int8x16.extractLane(c, 11));
    equal(3, SIMD.Int8x16.extractLane(c, 12));
    equal(2, SIMD.Int8x16.extractLane(c, 13));
    equal(1, SIMD.Int8x16.extractLane(c, 14));
    equal(0, SIMD.Int8x16.extractLane(c, 15));

    c = SIMD.Int8x16.not(SIMD.Int8x16(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1));
    equal(-17, SIMD.Int8x16.extractLane(c, 0));
    equal(-16, SIMD.Int8x16.extractLane(c, 1));
    equal(-15, SIMD.Int8x16.extractLane(c, 2));
    equal(-14, SIMD.Int8x16.extractLane(c, 3));
    equal(-13, SIMD.Int8x16.extractLane(c, 4));
    equal(-12, SIMD.Int8x16.extractLane(c, 5));
    equal(-11, SIMD.Int8x16.extractLane(c, 6));
    equal(-10, SIMD.Int8x16.extractLane(c, 7));
    equal(-9, SIMD.Int8x16.extractLane(c, 8));
    equal(-8, SIMD.Int8x16.extractLane(c, 9));
    equal(-7, SIMD.Int8x16.extractLane(c, 10));
    equal(-6, SIMD.Int8x16.extractLane(c, 11));
    equal(-5, SIMD.Int8x16.extractLane(c, 12));
    equal(-4, SIMD.Int8x16.extractLane(c, 13));
    equal(-3, SIMD.Int8x16.extractLane(c, 14));
    equal(-2, SIMD.Int8x16.extractLane(c, 15));

}

testNot();
testNot();
testNot();
testNot();
testNot();
testNot();
testNot();
testNot();