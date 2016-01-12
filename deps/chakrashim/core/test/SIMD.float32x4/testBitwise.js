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

function equalNaN(a) {
    if (isNaN(a))
    {
        print("Correct");
    }
    else
    {
        print(">> Fail!");
    }
}
function testOr() {
    print("Float32x4 Or");
    var a = SIMD.Float32x4(4.0, 3.0, 2.0, 1.0);
    var b = SIMD.Float32x4(10.0, 20.0, 30.0, 40.0);
    var c = SIMD.Float32x4.or(a, b);
    equal(20, SIMD.Float32x4.extractLane(c, 0));
    equal(28, SIMD.Float32x4.extractLane(c, 1));
    equal(30, SIMD.Float32x4.extractLane(c, 2));
    equalNaN(SIMD.Float32x4.extractLane(c, 3));
}

function testNot() {
    print("Float32x4 Not");
    var a = SIMD.Float32x4(4.0, 3.0, 2.0, 1.0);
    var b = SIMD.Float32x4(10.0, 20.0, 30.0, 40.0);
    var c = SIMD.Float32x4.not(a, b);
    equal(-0.9999999403953552, SIMD.Float32x4.extractLane(c, 0));
    equal(-1.4999998807907104, SIMD.Float32x4.extractLane(c, 1));
    equal(-1.9999998807907104, SIMD.Float32x4.extractLane(c, 2));
    equal(-3.999999761581421, SIMD.Float32x4.extractLane(c, 3));
}

function testAnd() {
    print("Float32x4 And");
    var a = SIMD.Float32x4(4.0, 3.0, 2.0, 1.0);
    var b = SIMD.Float32x4(10.0, 20.0, 30.0, 40.0);
    var c = SIMD.Float32x4.and(a, b);

    equal(2, SIMD.Float32x4.extractLane(c, 0));
    equal(2, SIMD.Float32x4.extractLane(c, 1));
    equal(2, SIMD.Float32x4.extractLane(c, 2));
    equal(9.4039548065783e-38, SIMD.Float32x4.extractLane(c, 3));
}

function testXor() {
    print("Float32x4 Xor");
    var a = SIMD.Float32x4(4.0, 3.0, 2.0, 1.0);
    var b = SIMD.Float32x4(10.0, 20.0, 30.0, 40.0);
    var c = SIMD.Float32x4.xor(a, b);

    equal(5.877471754111438e-38, SIMD.Float32x4.extractLane(c, 0));
    equal(8.228460455756013e-38, SIMD.Float32x4.extractLane(c, 1));
    equal(8.816207631167156e-38, SIMD.Float32x4.extractLane(c, 2));
    equal(2.6584559915698317e+37, SIMD.Float32x4.extractLane(c, 3));
}

testOr();
testOr();
testOr();
testOr();
testOr();
testOr();
testOr();
testOr();


testXor();
testXor();
testXor();
testXor();
testXor();
testXor();
testXor();
testXor();

testAnd();
testAnd();
testAnd();
testAnd();
testAnd();
testAnd();
testAnd();
testAnd();

testNot();
testNot();
testNot();
testNot();
testNot();
testNot();
testNot();
testNot();