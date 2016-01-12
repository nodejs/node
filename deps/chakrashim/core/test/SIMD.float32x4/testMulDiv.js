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
    print("Float32x4 mul");
    var a = SIMD.Float32x4(4.0, 3.0, 2.0, 1.0);
    var b = SIMD.Float32x4(10.0, 20.0, 30.0, 40.0);
    var c = SIMD.Float32x4.mul(a, b);
    equal(40.0, SIMD.Float32x4.extractLane(c, 0));
    equal(60.0, SIMD.Float32x4.extractLane(c, 1));
    equal(60.0, SIMD.Float32x4.extractLane(c, 2));
    equal(40.0, SIMD.Float32x4.extractLane(c, 3));
}

function testDiv() {
    print("Float32x4 div");
    var a = SIMD.Float32x4(4.0, 9.0, 8.0, 1.0);
    var b = SIMD.Float32x4(2.0, 3.0, 1.0, 0.5);
    var c = SIMD.Float32x4.div(a, b);
    equal(2.0, SIMD.Float32x4.extractLane(c, 0));
    equal(3.0, SIMD.Float32x4.extractLane(c, 1));
    equal(8.0, SIMD.Float32x4.extractLane(c, 2));
    equal(2.0, SIMD.Float32x4.extractLane(c, 3));
}

testMul();
testMul();
testMul();
testMul();
testMul();
testMul();
testMul();
testMul();

testDiv();
testDiv();
testDiv();
testDiv();
testDiv();
testDiv();
testDiv();
testDiv();