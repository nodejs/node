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

function almostEqual(a, b) {
    if (Math.abs(a - b) < 0.00001) {
        print("Correct");
    } else {
        print(">> Fail!");
    }
}

function testReciprocal() {
    print("Float32x4 Reciprocal");
    var a = SIMD.Float32x4(8.0, 4.0, 2.0, -2.0);
    var c = SIMD.Float32x4.reciprocal(a);

    equal(0.125, SIMD.Float32x4.extractLane(c, 0));
    equal(0.250, SIMD.Float32x4.extractLane(c, 1));
    equal(0.5, SIMD.Float32x4.extractLane(c, 2));
    equal(-0.5, SIMD.Float32x4.extractLane(c, 3));
}

function testReciprocalSqrt() {
    print("Float32x4 ReciprocalSqrt");
    var a = SIMD.Float32x4(1.0, 0.25, 0.111111, 0.0625);
    var c = SIMD.Float32x4.reciprocalSqrt(a);
    almostEqual(1.0, SIMD.Float32x4.extractLane(c, 0));
    almostEqual(2.0, SIMD.Float32x4.extractLane(c, 1));
    almostEqual(3.0, SIMD.Float32x4.extractLane(c, 2));
    almostEqual(4.0, SIMD.Float32x4.extractLane(c, 3));
}


testReciprocal();
testReciprocal();
testReciprocal();
testReciprocal();
testReciprocal();
testReciprocal();
testReciprocal();

testReciprocalSqrt();
testReciprocalSqrt();
testReciprocalSqrt();
testReciprocalSqrt();
testReciprocalSqrt();
testReciprocalSqrt();
testReciprocalSqrt();
testReciprocalSqrt();