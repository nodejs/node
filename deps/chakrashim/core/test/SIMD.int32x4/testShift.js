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

function testShiftleft() {
    print("Int32x4 shiftLeft");
    var a = SIMD.Int32x4(0x80000000, 0x7000000, 0xFFFFFFFF, 0x0);
    var b = SIMD.Int32x4.shiftLeft(a, 1)
    equal(0x0, SIMD.Int32x4.extractLane(b, 0));
    equal(0xE000000, SIMD.Int32x4.extractLane(b, 1));
    equal(-2, SIMD.Int32x4.extractLane(b, 2));
    equal(0x0, SIMD.Int32x4.extractLane(b, 3));
    var c = SIMD.Int32x4(1, 2, 3, 4);
    var d = SIMD.Int32x4.shiftLeft(c, 1)
    equal(2, SIMD.Int32x4.extractLane(d, 0));
    equal(4, SIMD.Int32x4.extractLane(d, 1));
    equal(6, SIMD.Int32x4.extractLane(d, 2));
    equal(8, SIMD.Int32x4.extractLane(d, 3));
}

function testShiftRightLogical() {
    print("Int32x4 shiftRightLogical");
    var a = SIMD.Int32x4(0x80000000, 0x7000000, 0xFFFFFFFF, 0x0);
    var b = SIMD.Int32x4.shiftRightLogical(a, 1)
    equal(0x40000000, SIMD.Int32x4.extractLane(b, 0));
    equal(0x03800000, SIMD.Int32x4.extractLane(b, 1));
    equal(0x7FFFFFFF, SIMD.Int32x4.extractLane(b, 2));
    equal(0x0, SIMD.Int32x4.extractLane(b, 3));
    var c = SIMD.Int32x4(1, 2, 3, 4);
    var d = SIMD.Int32x4.shiftRightLogical(c, 1)
    equal(0, SIMD.Int32x4.extractLane(d, 0));
    equal(1, SIMD.Int32x4.extractLane(d, 1));
    equal(1, SIMD.Int32x4.extractLane(d, 2));
    equal(2, SIMD.Int32x4.extractLane(d, 3));
}

function testShiftRightArithmetic() {
    print("Int32x4 shiftRightArithmetic");
    var a = SIMD.Int32x4(0x80000000, 0x7000000, 0xFFFFFFFF, 0x0);
    var b = SIMD.Int32x4.shiftRightArithmetic(a, 1)
    equal(-1073741824, SIMD.Int32x4.extractLane(b, 0));
    //equal(0xC0000000, SIMD.Int32x4.extractLane(b, 0));
    equal(0x03800000, SIMD.Int32x4.extractLane(b, 1));
    equal(-1, SIMD.Int32x4.extractLane(b, 2));
    equal(0x0, SIMD.Int32x4.extractLane(b, 3));
    var c = SIMD.Int32x4(1, 2, 3, 4);
    var d = SIMD.Int32x4.shiftRightArithmetic(c, 1)
    equal(0, SIMD.Int32x4.extractLane(d, 0));
    equal(1, SIMD.Int32x4.extractLane(d, 1));
    equal(1, SIMD.Int32x4.extractLane(d, 2));
    equal(2, SIMD.Int32x4.extractLane(d, 3));
    var c = SIMD.Int32x4(-1, -2, -3, -4);
    var d = SIMD.Int32x4.shiftRightArithmetic(c, 1)
    equal(-1, SIMD.Int32x4.extractLane(d, 0));
    equal(-1, SIMD.Int32x4.extractLane(d, 1));
    equal(-2, SIMD.Int32x4.extractLane(d, 2));
    equal(-2, SIMD.Int32x4.extractLane(d, 3));
}

testShiftleft();
testShiftleft();
testShiftleft();
testShiftleft();

testShiftRightLogical();
testShiftRightLogical();
testShiftRightLogical();
testShiftRightLogical();

testShiftRightArithmetic();
testShiftRightArithmetic();
testShiftRightArithmetic();
testShiftRightArithmetic();