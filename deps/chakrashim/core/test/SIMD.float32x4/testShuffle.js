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
    print("Float32x4 Shuffle");
    var a = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);
    var xyxy = SIMD.Float32x4.swizzle(a, 0, 1, 0, 1);
    var zwzw = SIMD.Float32x4.swizzle(a, 2, 3, 2, 3);
    var xxxx = SIMD.Float32x4.swizzle(a, 0, 0, 0, 0);
    equal(1.0, SIMD.Float32x4.extractLane(xyxy, 0));
    equal(2.0, SIMD.Float32x4.extractLane(xyxy, 1));
    equal(1.0, SIMD.Float32x4.extractLane(xyxy, 2));
    equal(2.0, SIMD.Float32x4.extractLane(xyxy, 3));
    equal(3.0, SIMD.Float32x4.extractLane(zwzw, 0));
    equal(4.0, SIMD.Float32x4.extractLane(zwzw, 1));
    equal(3.0, SIMD.Float32x4.extractLane(zwzw, 2));
    equal(4.0, SIMD.Float32x4.extractLane(zwzw, 3));
    equal(1.0, SIMD.Float32x4.extractLane(xxxx, 0));
    equal(1.0, SIMD.Float32x4.extractLane(xxxx, 1));
    equal(1.0, SIMD.Float32x4.extractLane(xxxx, 2));
    equal(1.0, SIMD.Float32x4.extractLane(xxxx, 3));
}

function testShuffle() {
    print("Float32x4 ShuffleMix");
    var a = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);
    var b = SIMD.Float32x4(5.0, 6.0, 7.0, 8.0);
    var xyxy = SIMD.Float32x4.shuffle(a, b, 0, 1, 4, 5);
    var zwzw = SIMD.Float32x4.shuffle(a, b, 2, 3, 6, 7);
    var xxxx = SIMD.Float32x4.shuffle(a, b, 0, 0, 4, 4);
    equal(1.0, SIMD.Float32x4.extractLane(xyxy, 0));
    equal(2.0, SIMD.Float32x4.extractLane(xyxy, 1));
    equal(5.0, SIMD.Float32x4.extractLane(xyxy, 2));
    equal(6.0, SIMD.Float32x4.extractLane(xyxy, 3));
    equal(3.0, SIMD.Float32x4.extractLane(zwzw, 0));
    equal(4.0, SIMD.Float32x4.extractLane(zwzw, 1));
    equal(7.0, SIMD.Float32x4.extractLane(zwzw, 2));
    equal(8.0, SIMD.Float32x4.extractLane(zwzw, 3));
    equal(1.0, SIMD.Float32x4.extractLane(xxxx, 0));
    equal(1.0, SIMD.Float32x4.extractLane(xxxx, 1));
    equal(5.0, SIMD.Float32x4.extractLane(xxxx, 2));
    equal(5.0, SIMD.Float32x4.extractLane(xxxx, 3));
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