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

function testSwizzle() {
    print("Int32x4 Shuffle");
    var a = SIMD.Int32x4(1, 2, 3, 4);
    var xyxy = SIMD.Int32x4.swizzle(a, 0, 1, 0, 1);
    var zwzw = SIMD.Int32x4.swizzle(a, 2, 3, 2, 3);
    var xxxx = SIMD.Int32x4.swizzle(a, 0, 0, 0, 0);
    equal(1, SIMD.Int32x4.extractLane(xyxy, 0));
    equal(2, SIMD.Int32x4.extractLane(xyxy, 1));
    equal(1, SIMD.Int32x4.extractLane(xyxy, 2));
    equal(2, SIMD.Int32x4.extractLane(xyxy, 3));
    equal(3, SIMD.Int32x4.extractLane(zwzw, 0));
    equal(4, SIMD.Int32x4.extractLane(zwzw, 1));
    equal(3, SIMD.Int32x4.extractLane(zwzw, 2));
    equal(4, SIMD.Int32x4.extractLane(zwzw, 3));
    equal(1, SIMD.Int32x4.extractLane(xxxx, 0));
    equal(1, SIMD.Int32x4.extractLane(xxxx, 1));
    equal(1, SIMD.Int32x4.extractLane(xxxx, 2));
    equal(1, SIMD.Int32x4.extractLane(xxxx, 3));
}

function testShuffle() {
    print("Int32x4 ShuffleMix");
    var a = SIMD.Int32x4(1, 2, 3, 4);
    var b = SIMD.Int32x4(5, 6, 7, 8);
    var xyxy = SIMD.Int32x4.shuffle(a, b, 0, 1, 4, 5);
    var zwzw = SIMD.Int32x4.shuffle(a, b, 2, 3, 6, 7);
    var xxxx = SIMD.Int32x4.shuffle(a, b, 0, 0, 4, 4);
    equal(1, SIMD.Int32x4.extractLane(xyxy, 0));
    equal(2, SIMD.Int32x4.extractLane(xyxy, 1));
    equal(5, SIMD.Int32x4.extractLane(xyxy, 2));
    equal(6, SIMD.Int32x4.extractLane(xyxy, 3));
    equal(3, SIMD.Int32x4.extractLane(zwzw, 0));
    equal(4, SIMD.Int32x4.extractLane(zwzw, 1));
    equal(7, SIMD.Int32x4.extractLane(zwzw, 2));
    equal(8, SIMD.Int32x4.extractLane(zwzw, 3));
    equal(1, SIMD.Int32x4.extractLane(xxxx, 0));
    equal(1, SIMD.Int32x4.extractLane(xxxx, 1));
    equal(5, SIMD.Int32x4.extractLane(xxxx, 2));
    equal(5, SIMD.Int32x4.extractLane(xxxx, 3));
}

testSwizzle();
testSwizzle();
testSwizzle();
testSwizzle();
testSwizzle();
testSwizzle();
testSwizzle();
testSwizzle();

testShuffle();
testShuffle();
testShuffle();
testShuffle();
testShuffle();
testShuffle();
testShuffle();
testShuffle();