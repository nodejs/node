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

function testScale() {
    print("Float32x4 Scale");
    var a = SIMD.Float32x4(8.0, 4.0, 2.0, -2.0);
    var c = SIMD.Float32x4.scale(a, 0.5);
    equal(4.0, SIMD.Float32x4.extractLane(c, 0));
    equal(2.0, SIMD.Float32x4.extractLane(c, 1));
    equal(1.0, SIMD.Float32x4.extractLane(c, 2));
    equal(-1.0, SIMD.Float32x4.extractLane(c, 3));
}

testScale();
testScale();
testScale();
testScale();
testScale();
testScale();
testScale();