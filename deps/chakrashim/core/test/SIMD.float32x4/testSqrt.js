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

function testSqrt() {
    print("Float32x4 Sqrt");
    var a = SIMD.Float32x4(16.0, 9.0, 4.0, 1.0);
    var c = SIMD.Float32x4.sqrt(a);
    equal(4.0, SIMD.Float32x4.extractLane(c, 0));
    equal(3.0, SIMD.Float32x4.extractLane(c, 1));
    equal(2.0, SIMD.Float32x4.extractLane(c, 2));
    equal(1.0, SIMD.Float32x4.extractLane(c, 3));
}

testSqrt();
testSqrt();
testSqrt();
testSqrt();
testSqrt();
testSqrt();
testSqrt();