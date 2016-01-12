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

function testSelect() {
    print("Int32x4 Select");
    var m = SIMD.Int32x4.bool(true, true, false, false);
    var t = SIMD.Int32x4(1, 2, 3, 4);
    var f = SIMD.Int32x4(5, 6, 7, 8);
    var s = SIMD.Int32x4.select(m, t, f);
    equal(1, SIMD.Int32x4.extractLane(s, 0));
    equal(2, SIMD.Int32x4.extractLane(s, 1));
    equal(7, SIMD.Int32x4.extractLane(s, 2));
    equal(8, SIMD.Int32x4.extractLane(s, 3));
}

testSelect();
testSelect();
testSelect();
testSelect();
testSelect();
testSelect();
testSelect();