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

function testNot() {
    print("Int32x4 not");
    var a = SIMD.Int32x4(-4, -3, -2, -1);
    var c = SIMD.Int32x4.not(a);
    equal(3, SIMD.Int32x4.extractLane(c, 0));
    equal(2, SIMD.Int32x4.extractLane(c, 1));
    equal(1, SIMD.Int32x4.extractLane(c, 2));
    equal(0, SIMD.Int32x4.extractLane(c, 3));
    c = SIMD.Int32x4.not(SIMD.Int32x4(4, 3, 2, 1));
    equal(-5, SIMD.Int32x4.extractLane(c, 0));
    equal(-4, SIMD.Int32x4.extractLane(c, 1));
    equal(-3, SIMD.Int32x4.extractLane(c, 2));
    equal(-2, SIMD.Int32x4.extractLane(c, 3));
}

testNot();
testNot();
testNot();
testNot();
testNot();
testNot();
testNot();
testNot();