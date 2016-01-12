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

function testAnd() {
    print("Int8x16 and");
    var m = SIMD.Int8x16(0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA);
    var n = SIMD.Int8x16(0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55);
    equal(-86, SIMD.Int8x16.extractLane(m, 0));
    equal(-86, SIMD.Int8x16.extractLane(m, 1));
    equal(-86, SIMD.Int8x16.extractLane(m, 2));
    equal(-86, SIMD.Int8x16.extractLane(m, 3));
    equal(-86, SIMD.Int8x16.extractLane(m, 4));
    equal(-86, SIMD.Int8x16.extractLane(m, 5));
    equal(-86, SIMD.Int8x16.extractLane(m, 6));
    equal(-86, SIMD.Int8x16.extractLane(m, 7));
    equal(-86, SIMD.Int8x16.extractLane(m, 8));
    equal(-86, SIMD.Int8x16.extractLane(m, 9));
    equal(-86, SIMD.Int8x16.extractLane(m, 10));
    equal(-86, SIMD.Int8x16.extractLane(m, 11));
    equal(-86, SIMD.Int8x16.extractLane(m, 12));
    equal(-86, SIMD.Int8x16.extractLane(m, 13));
    equal(-86, SIMD.Int8x16.extractLane(m, 14));
    equal(-86, SIMD.Int8x16.extractLane(m, 15));
    equal(0x55, SIMD.Int8x16.extractLane(n, 0));
    equal(0x55, SIMD.Int8x16.extractLane(n, 1));
    equal(0x55, SIMD.Int8x16.extractLane(n, 2));
    equal(0x55, SIMD.Int8x16.extractLane(n, 3));
    equal(0x55, SIMD.Int8x16.extractLane(n, 4));
    equal(0x55, SIMD.Int8x16.extractLane(n, 5));
    equal(0x55, SIMD.Int8x16.extractLane(n, 6));
    equal(0x55, SIMD.Int8x16.extractLane(n, 7));
    equal(0x55, SIMD.Int8x16.extractLane(n, 8));
    equal(0x55, SIMD.Int8x16.extractLane(n, 9));
    equal(0x55, SIMD.Int8x16.extractLane(n, 10));
    equal(0x55, SIMD.Int8x16.extractLane(n, 11));
    equal(0x55, SIMD.Int8x16.extractLane(n, 12));
    equal(0x55, SIMD.Int8x16.extractLane(n, 13));
    equal(0x55, SIMD.Int8x16.extractLane(n, 14));
    equal(0x55, SIMD.Int8x16.extractLane(n, 15));
    var o = SIMD.Int8x16.and(m, n);  // and
    equal(0x0, SIMD.Int8x16.extractLane(o, 0));
    equal(0x0, SIMD.Int8x16.extractLane(o, 1));
    equal(0x0, SIMD.Int8x16.extractLane(o, 2));
    equal(0x0, SIMD.Int8x16.extractLane(o, 3));
    equal(0x0, SIMD.Int8x16.extractLane(o, 4));
    equal(0x0, SIMD.Int8x16.extractLane(o, 5));
    equal(0x0, SIMD.Int8x16.extractLane(o, 6));
    equal(0x0, SIMD.Int8x16.extractLane(o, 7));
    equal(0x0, SIMD.Int8x16.extractLane(o, 8));
    equal(0x0, SIMD.Int8x16.extractLane(o, 9));
    equal(0x0, SIMD.Int8x16.extractLane(o, 10));
    equal(0x0, SIMD.Int8x16.extractLane(o, 11));
    equal(0x0, SIMD.Int8x16.extractLane(o, 12));
    equal(0x0, SIMD.Int8x16.extractLane(o, 13));
    equal(0x0, SIMD.Int8x16.extractLane(o, 14));
    equal(0x0, SIMD.Int8x16.extractLane(o, 15));
}

function testOr() {
    print("Int8x16 or");
    var m = SIMD.Int8x16(0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA);
    var n = SIMD.Int8x16(0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55);
    var o = SIMD.Int8x16.or(m, n);  // or
    equal(-1, SIMD.Int8x16.extractLane(o, 0));
    equal(-1, SIMD.Int8x16.extractLane(o, 1));
    equal(-1, SIMD.Int8x16.extractLane(o, 2));
    equal(-1, SIMD.Int8x16.extractLane(o, 3));
    equal(-1, SIMD.Int8x16.extractLane(o, 4));
    equal(-1, SIMD.Int8x16.extractLane(o, 5));
    equal(-1, SIMD.Int8x16.extractLane(o, 6));
    equal(-1, SIMD.Int8x16.extractLane(o, 7));
    equal(-1, SIMD.Int8x16.extractLane(o, 8));
    equal(-1, SIMD.Int8x16.extractLane(o, 9));
    equal(-1, SIMD.Int8x16.extractLane(o, 10));
    equal(-1, SIMD.Int8x16.extractLane(o, 11));
    equal(-1, SIMD.Int8x16.extractLane(o, 12));
    equal(-1, SIMD.Int8x16.extractLane(o, 13));
    equal(-1, SIMD.Int8x16.extractLane(o, 14));
    equal(-1, SIMD.Int8x16.extractLane(o, 15));
}

function testXor() {
    var m = SIMD.Int8x16(0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA)
    var o = SIMD.Int8x16.xor(m, m);  // xor
    equal(0x0, SIMD.Int8x16.extractLane(o, 0));
    equal(0x0, SIMD.Int8x16.extractLane(o, 1));
    equal(0x0, SIMD.Int8x16.extractLane(o, 2));
    equal(0x0, SIMD.Int8x16.extractLane(o, 3));
    equal(0x0, SIMD.Int8x16.extractLane(o, 4));
    equal(0x0, SIMD.Int8x16.extractLane(o, 5));
    equal(0x0, SIMD.Int8x16.extractLane(o, 6));
    equal(0x0, SIMD.Int8x16.extractLane(o, 7));
    equal(0x0, SIMD.Int8x16.extractLane(o, 8));
    equal(0x0, SIMD.Int8x16.extractLane(o, 9));
    equal(0x0, SIMD.Int8x16.extractLane(o, 10));
    equal(0x0, SIMD.Int8x16.extractLane(o, 11));
    equal(0x0, SIMD.Int8x16.extractLane(o, 12));
    equal(0x0, SIMD.Int8x16.extractLane(o, 13));
    equal(0x0, SIMD.Int8x16.extractLane(o, 14));
    equal(0x0, SIMD.Int8x16.extractLane(o, 15));

}

testAnd();
testAnd();
testAnd();
testAnd();

testOr();
testOr();
testOr();
testOr();

testXor();
testXor();
testXor();
testXor();