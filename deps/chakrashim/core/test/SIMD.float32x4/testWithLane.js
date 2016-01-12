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

function equalNaN(a) {
    if (isNaN(a)) {
        print("Correct");
    } else {
        print(">> Fail!");
    }
}

function testReplaceLane1() {
    print("Float32x4 Lane1");
    var a = SIMD.Float32x4(16.0, 9.0, 4.0, 1.0);
    var c = SIMD.Float32x4.replaceLane(a, 0, 20.0);
    equal(20.0, SIMD.Float32x4.extractLane(c, 0));
    equal(9.0, SIMD.Float32x4.extractLane(c, 1));
    equal(4.0, SIMD.Float32x4.extractLane(c, 2));
    equal(1.0, SIMD.Float32x4.extractLane(c, 3));

}

function testReplaceLane2() {
    print("Float32x4 Lane2");
    var a = SIMD.Float32x4(16.0, 9.0, 4.0, 1.0);
    var c = SIMD.Float32x4.replaceLane(a, 1, 20.0);

    equal(16.0, SIMD.Float32x4.extractLane(c, 0));
    equal(20.0, SIMD.Float32x4.extractLane(c, 1));
    equal(4.0, SIMD.Float32x4.extractLane(c, 2));
    equal(1.0, SIMD.Float32x4.extractLane(c, 3));

}

function testReplaceLane3() {
    print("Float32x4 Lane3");
    var a = SIMD.Float32x4(16.0, 9.0, 4.0, 1.0);
    var c = SIMD.Float32x4.replaceLane(a, 2, 20.0);

    equal(16.0, SIMD.Float32x4.extractLane(c, 0));
    equal(9.0, SIMD.Float32x4.extractLane(c, 1));
    equal(20.0, SIMD.Float32x4.extractLane(c, 2));
    equal(1.0, SIMD.Float32x4.extractLane(c, 3));

}

function testReplaceLane4() {
    print("Float32x4 Lane4");
    var a = SIMD.Float32x4(16.0, 9.0, 4.0, 1.0);
    var c = SIMD.Float32x4.replaceLane(a, 3, 20.0);

    equal(16.0, SIMD.Float32x4.extractLane(c, 0));
    equal(9.0, SIMD.Float32x4.extractLane(c, 1));
    equal(4.0, SIMD.Float32x4.extractLane(c, 2));
    equal(20.0, SIMD.Float32x4.extractLane(c, 3));

}

testReplaceLane1();
testReplaceLane1();
testReplaceLane1();
testReplaceLane1();
testReplaceLane1();
testReplaceLane1();
testReplaceLane1();
testReplaceLane1();

testReplaceLane2();
testReplaceLane2();
testReplaceLane2();
testReplaceLane2();
testReplaceLane2();
testReplaceLane2();
testReplaceLane2();
testReplaceLane2();

testReplaceLane3();
testReplaceLane3();
testReplaceLane3();
testReplaceLane3();
testReplaceLane3();
testReplaceLane3();
testReplaceLane3();
testReplaceLane3();

testReplaceLane4();
testReplaceLane4();
testReplaceLane4();
testReplaceLane4();
testReplaceLane4();
testReplaceLane4();
testReplaceLane4();
testReplaceLane4();
