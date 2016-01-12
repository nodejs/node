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

function testClamp() {
    print("Float32x4 clamp");
    var a = SIMD.Float32x4(-20.0, 10.0, 30.0, 0.5);
    var lower = SIMD.Float32x4(2.0, 1.0, 50.0, 0.0);
    var upper = SIMD.Float32x4(2.5, 5.0, 55.0, 1.0);
    var c = SIMD.Float32x4.clamp(a, lower, upper);
    equal(2.0, SIMD.Float32x4.extractLane(c, 0));
    equal(5.0, SIMD.Float32x4.extractLane(c, 1));
    equal(50.0, SIMD.Float32x4.extractLane(c, 2));
    equal(0.5, SIMD.Float32x4.extractLane(c, 3));
}

testClamp();
testClamp();
testClamp();
testClamp();
testClamp();
testClamp();
testClamp();
testClamp();