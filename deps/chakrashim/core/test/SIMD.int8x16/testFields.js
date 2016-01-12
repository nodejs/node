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

function testScalarGetters() {
    print('Int8x16 scalar getters');
    var a = SIMD.Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    equal(1, SIMD.Int8x16.extractLane(a, 0));
    equal(2, SIMD.Int8x16.extractLane(a, 1));
    equal(3, SIMD.Int8x16.extractLane(a, 2));
    equal(4, SIMD.Int8x16.extractLane(a, 3));
    equal(5, SIMD.Int8x16.extractLane(a, 4));
    equal(6, SIMD.Int8x16.extractLane(a, 5));
    equal(7, SIMD.Int8x16.extractLane(a, 6));
    equal(8, SIMD.Int8x16.extractLane(a, 7));
    equal(9, SIMD.Int8x16.extractLane(a, 8));
    equal(10, SIMD.Int8x16.extractLane(a, 9));
    equal(11, SIMD.Int8x16.extractLane(a, 10));
    equal(12, SIMD.Int8x16.extractLane(a, 11));
    equal(13, SIMD.Int8x16.extractLane(a, 12));
    equal(14, SIMD.Int8x16.extractLane(a, 13));
    equal(15, SIMD.Int8x16.extractLane(a, 14));
    equal(16, SIMD.Int8x16.extractLane(a, 15));

}

testScalarGetters();
testScalarGetters();
testScalarGetters();
testScalarGetters();
testScalarGetters();
testScalarGetters();
testScalarGetters();
testScalarGetters();