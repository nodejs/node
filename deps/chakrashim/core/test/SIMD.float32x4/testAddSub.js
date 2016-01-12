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

function testAdd() {
    print("Float32x4 add");
    var a = SIMD.Float32x4(4.0, 3.0, 2.0, 1.0);
    var b = SIMD.Float32x4(10.0, 20.0, 30.0, 40.0);
    var c = SIMD.Float32x4.add(a, b);
    equal(14.0, SIMD.Float32x4.extractLane(c, 0));
    equal(23.0, SIMD.Float32x4.extractLane(c, 1));
    equal(32.0, SIMD.Float32x4.extractLane(c, 2));
    equal(41.0, SIMD.Float32x4.extractLane(c, 3));
}

function testSub() {
    print("Float32x4 sub");
    var a = SIMD.Float32x4(4.0, 3.0, 2.0, 1.0);
    var b = SIMD.Float32x4(10.0, 20.0, 30.0, 40.0);
    var c = SIMD.Float32x4.sub(a, b);
    equal(-6.0, SIMD.Float32x4.extractLane(c, 0));
    equal(-17.0, SIMD.Float32x4.extractLane(c, 1));
    equal(-28.0, SIMD.Float32x4.extractLane(c, 2));
    equal(-39.0, SIMD.Float32x4.extractLane(c, 3));
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