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

function testAdd() {
    print("Int8x16 add");
    var a = SIMD.Int8x16(0xFF, 0xFF, 0x7f, 0x0, 0xFF, 0xFF, 0x7f, 0x0, 0xFF, 0xFF, 0x7f, 0x0, 0xFF, 0xFF, 0x7f, 0x0);
    var b = SIMD.Int8x16(0x1, 0xFF, 0x1, 0xFF, 0x1, 0xFF, 0x1, 0xFF, 0x1, 0xFF, 0x1, 0xFF, 0x1, 0xFF, 0x1, 0xFF);
    var c = SIMD.Int8x16.add(a, b);
    equal(0x0, SIMD.Int8x16.extractLane(c, 0));
    equal(-2, SIMD.Int8x16.extractLane(c, 1));
    equal(-0x80, SIMD.Int8x16.extractLane(c, 2));
    equal(-1, SIMD.Int8x16.extractLane(c, 3));
    equal(0x0, SIMD.Int8x16.extractLane(c, 4));
    equal(-2, SIMD.Int8x16.extractLane(c, 5));
    equal(-0x80, SIMD.Int8x16.extractLane(c, 6));
    equal(-1, SIMD.Int8x16.extractLane(c, 7));
    equal(0x0, SIMD.Int8x16.extractLane(c, 8));
    equal(-2, SIMD.Int8x16.extractLane(c, 9));
    equal(-0x80, SIMD.Int8x16.extractLane(c, 10));
    equal(-1, SIMD.Int8x16.extractLane(c, 11));
    equal(0x0, SIMD.Int8x16.extractLane(c, 12));
    equal(-2, SIMD.Int8x16.extractLane(c, 13));
    equal(-0x80, SIMD.Int8x16.extractLane(c, 14));
    equal(-1, SIMD.Int8x16.extractLane(c, 15));

    var m = SIMD.Int8x16(4, 3, 2, 1, 4, 3, 2, 1, 4, 3, 2, 1, 4, 3, 2, 1);
    var n = SIMD.Int8x16(10, 20, 30, 40, 10, 20, 30, 40, 10, 20, 30, 40, 10, 20, 30, 40);
    var f = SIMD.Int8x16.add(m, n);
    equal(14, SIMD.Int8x16.extractLane(f, 0));
    equal(23, SIMD.Int8x16.extractLane(f, 1));
    equal(32, SIMD.Int8x16.extractLane(f, 2));
    equal(41, SIMD.Int8x16.extractLane(f, 3));
    equal(14, SIMD.Int8x16.extractLane(f, 4));
    equal(23, SIMD.Int8x16.extractLane(f, 5));
    equal(32, SIMD.Int8x16.extractLane(f, 6));
    equal(41, SIMD.Int8x16.extractLane(f, 7));
    equal(14, SIMD.Int8x16.extractLane(f, 8));
    equal(23, SIMD.Int8x16.extractLane(f, 9));
    equal(32, SIMD.Int8x16.extractLane(f, 10));
    equal(41, SIMD.Int8x16.extractLane(f, 11));
    equal(14, SIMD.Int8x16.extractLane(f, 12));
    equal(23, SIMD.Int8x16.extractLane(f, 13));
    equal(32, SIMD.Int8x16.extractLane(f, 14));
    equal(41, SIMD.Int8x16.extractLane(f, 15));

}

function testSub() {
    print("Int8x16 sub");
    var a = SIMD.Int8x16(0xFF, 0xFF, 0x80, 0x0, 0xFF, 0xFF, 0x80, 0x0, 0xFF, 0xFF, 0x80, 0x0, 0xFF, 0xFF, 0x80, 0x0);
    var b = SIMD.Int8x16(0x1, 0xFF, 0x1, 0xFF, 0x1, 0xFF, 0x1, 0xFF, 0x1, 0xFF, 0x1, 0xFF, 0x1, 0xFF, 0x1, 0xFF);
    var c = SIMD.Int8x16.sub(a, b);
    equal(-2, SIMD.Int8x16.extractLane(c, 0));
    equal(0x0, SIMD.Int8x16.extractLane(c, 1));
    equal(0x7f, SIMD.Int8x16.extractLane(c, 2));
    equal(0x1, SIMD.Int8x16.extractLane(c, 3));
    equal(-2, SIMD.Int8x16.extractLane(c, 4));
    equal(0x0, SIMD.Int8x16.extractLane(c, 5));
    equal(0x7f, SIMD.Int8x16.extractLane(c, 6));
    equal(0x1, SIMD.Int8x16.extractLane(c, 7));
    equal(-2, SIMD.Int8x16.extractLane(c, 8));
    equal(0x0, SIMD.Int8x16.extractLane(c, 9));
    equal(0x7f, SIMD.Int8x16.extractLane(c, 10));
    equal(0x1, SIMD.Int8x16.extractLane(c, 11));
    equal(-2, SIMD.Int8x16.extractLane(c, 12));
    equal(0x0, SIMD.Int8x16.extractLane(c, 13));
    equal(0x7f, SIMD.Int8x16.extractLane(c, 14));
    equal(0x1, SIMD.Int8x16.extractLane(c, 15));

    var d = SIMD.Int8x16(4, 3, 2, 1, 4, 3, 2, 1, 4, 3, 2, 1, 4, 3, 2, 1);
    var e = SIMD.Int8x16(10, 20, 30, 40, 10, 20, 30, 40, 10, 20, 30, 40, 10, 20, 30, 40);
    var f = SIMD.Int8x16.sub(d, e);
    equal(-6, SIMD.Int8x16.extractLane(f, 0));
    equal(-17, SIMD.Int8x16.extractLane(f, 1));
    equal(-28, SIMD.Int8x16.extractLane(f, 2));
    equal(-39, SIMD.Int8x16.extractLane(f, 3));
    equal(-6, SIMD.Int8x16.extractLane(f, 4));
    equal(-17, SIMD.Int8x16.extractLane(f, 5));
    equal(-28, SIMD.Int8x16.extractLane(f, 6));
    equal(-39, SIMD.Int8x16.extractLane(f, 7));
    equal(-6, SIMD.Int8x16.extractLane(f, 8));
    equal(-17, SIMD.Int8x16.extractLane(f, 9));
    equal(-28, SIMD.Int8x16.extractLane(f, 10));
    equal(-39, SIMD.Int8x16.extractLane(f, 11));
    equal(-6, SIMD.Int8x16.extractLane(f, 12));
    equal(-17, SIMD.Int8x16.extractLane(f, 13));
    equal(-28, SIMD.Int8x16.extractLane(f, 14));
    equal(-39, SIMD.Int8x16.extractLane(f, 15));

}

testAdd();
testAdd();
testAdd();
testAdd();
testAdd();
testAdd();
testAdd();

testSub();
testSub();
testSub();
testSub();
testSub();
testSub();
testSub();