//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function equal(a, b) {
    if (a == b)
        print("Correct");
    else
        print(">> Fail!");
}

function testZero() {
    print("zero");
    var z = SIMD.Int32x4.zero();
    equal(0, SIMD.Int32x4.extractLane(z, 0));
    equal(0, SIMD.Int32x4.extractLane(z, 1));
    equal(0, SIMD.Int32x4.extractLane(z, 2));
    equal(0, SIMD.Int32x4.extractLane(z, 3));
}

function testSplat() {
    var n = SIMD.Int32x4.splat(3);
    print("splat");
    equal(3, SIMD.Int32x4.extractLane(n, 0));
    equal(3, SIMD.Int32x4.extractLane(n, 1));
    equal(3, SIMD.Int32x4.extractLane(n, 2));
    equal(3, SIMD.Int32x4.extractLane(n, 3));
}

function testBool() {
    var n = SIMD.Int32x4.bool(true, false, true, false);
    print("bool");
    equal(-1, SIMD.Int32x4.extractLane(n, 0));
    equal(0, SIMD.Int32x4.extractLane(n, 1));
    equal(-1, SIMD.Int32x4.extractLane(n, 2));
    equal(0, SIMD.Int32x4.extractLane(n, 3));
}

testZero();
testZero();
testZero();
testZero();
testZero();
testZero();
testZero();
testZero();

testSplat();
testSplat();
testSplat();
testSplat();
testSplat();
testSplat();
testSplat();
testSplat();

testBool();
testBool();
testBool();
testBool();
testBool();
testBool();
testBool();
testBool();

