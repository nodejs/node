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
    print("Int32x4 add");
    var a = SIMD.Int32x4(0xFFFFFFFF, 0xFFFFFFFF, 0x7fffffff, 0x0);
    var b = SIMD.Int32x4(0x1, 0xFFFFFFFF, 0x1, 0xFFFFFFFF);
    var c = SIMD.Int32x4.add(a, b);
    equal(0x0, SIMD.Int32x4.extractLane(c, 0));
    equal(-2, SIMD.Int32x4.extractLane(c, 1));
    equal(-0x80000000, SIMD.Int32x4.extractLane(c, 2));
    equal(-1, SIMD.Int32x4.extractLane(c, 3));

    var m = SIMD.Int32x4(4, 3, 2, 1);
    var n = SIMD.Int32x4(10, 20, 30, 40);
    var f = SIMD.Int32x4.add(m, n);
    equal(14, SIMD.Int32x4.extractLane(f, 0));
    equal(23, SIMD.Int32x4.extractLane(f, 1));
    equal(32, SIMD.Int32x4.extractLane(f, 2));
    equal(41, SIMD.Int32x4.extractLane(f, 3));
}

function testSub() {
    print("Int32x4 sub");
    var a = SIMD.Int32x4(0xFFFFFFFF, 0xFFFFFFFF, 0x80000000, 0x0);
    var b = SIMD.Int32x4(0x1, 0xFFFFFFFF, 0x1, 0xFFFFFFFF);
    var c = SIMD.Int32x4.sub(a, b);
    equal(-2, SIMD.Int32x4.extractLane(c, 0));
    equal(0x0, SIMD.Int32x4.extractLane(c, 1));
    equal(0x7FFFFFFF, SIMD.Int32x4.extractLane(c, 2));
    equal(0x1, SIMD.Int32x4.extractLane(c, 3));

    var d = SIMD.Int32x4(4, 3, 2, 1);
    var e = SIMD.Int32x4(10, 20, 30, 40);
    var f = SIMD.Int32x4.sub(d, e);
    equal(-6, SIMD.Int32x4.extractLane(f, 0));
    equal(-17, SIMD.Int32x4.extractLane(f, 1));
    equal(-28, SIMD.Int32x4.extractLane(f, 2));
    equal(-39, SIMD.Int32x4.extractLane(f, 3));
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
