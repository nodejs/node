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

function testComparisons() {
    print("Int32x4 lessThan");
    var m = SIMD.Int32x4(1000, 2000, 100, 1);
    var n = SIMD.Int32x4(2000, 2000, 1, 100);
    var cmp;
    cmp = SIMD.Int32x4.lessThan(m, n);
    equal(-1, SIMD.Int32x4.extractLane(cmp, 0));
    equal(0x0, SIMD.Int32x4.extractLane(cmp, 1));
    equal(0x0, SIMD.Int32x4.extractLane(cmp, 2));
    equal(-1, SIMD.Int32x4.extractLane(cmp, 3));

    print("Int32x4 equal");
    cmp = SIMD.Int32x4.equal(m, n);
    equal(0x0, SIMD.Int32x4.extractLane(cmp, 0));
    equal(-1, SIMD.Int32x4.extractLane(cmp, 1));
    equal(0x0, SIMD.Int32x4.extractLane(cmp, 2));
    equal(0x0, SIMD.Int32x4.extractLane(cmp, 3));

    print("Int32x4 greaterThan");
    cmp = SIMD.Int32x4.greaterThan(m, n);
    equal(0x0, SIMD.Int32x4.extractLane(cmp, 0));
    equal(0x0, SIMD.Int32x4.extractLane(cmp, 1));
    equal(-1, SIMD.Int32x4.extractLane(cmp, 2));
    equal(0x0, SIMD.Int32x4.extractLane(cmp, 3));
}

testComparisons();
testComparisons();
testComparisons();
testComparisons();
testComparisons();
testComparisons();
testComparisons();
testComparisons();