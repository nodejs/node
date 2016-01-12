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

function testWithX() {
    print("Int8x16 replaceLane 0");
    var a = SIMD.Int8x16(16, 9, 4, 1, 16, 9, 4, 1, 16, 9, 4, 1, 16, 9, 4, 1);
    var c = SIMD.Int8x16.replaceLane(a, 0, 20);
    equal(20, SIMD.Int8x16.extractLane(c, 0));
    equal(9, SIMD.Int8x16.extractLane(c, 1));
    equal(4, SIMD.Int8x16.extractLane(c, 2));
    equal(1, SIMD.Int8x16.extractLane(c, 3));
    equal(16, SIMD.Int8x16.extractLane(c, 4));
    equal(9, SIMD.Int8x16.extractLane(c, 5));
    equal(4, SIMD.Int8x16.extractLane(c, 6));
    equal(1, SIMD.Int8x16.extractLane(c, 7));
    equal(16, SIMD.Int8x16.extractLane(c, 8));
    equal(9, SIMD.Int8x16.extractLane(c, 9));
    equal(4, SIMD.Int8x16.extractLane(c, 10));
    equal(1, SIMD.Int8x16.extractLane(c, 11));
    equal(16, SIMD.Int8x16.extractLane(c, 12));
    equal(9, SIMD.Int8x16.extractLane(c, 13));
    equal(4, SIMD.Int8x16.extractLane(c, 14));
    equal(1, SIMD.Int8x16.extractLane(c, 15));
    var m = SIMD.Int8x16(1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4);
    var n = SIMD.Int8x16.replaceLane(m, 0, 20);
    equal(20, SIMD.Int8x16.extractLane(n, 0));
    equal(2, SIMD.Int8x16.extractLane(n, 1));
    equal(3, SIMD.Int8x16.extractLane(n, 2));
    equal(4, SIMD.Int8x16.extractLane(n, 3));
    equal(1, SIMD.Int8x16.extractLane(n, 4));
    equal(2, SIMD.Int8x16.extractLane(n, 5));
    equal(3, SIMD.Int8x16.extractLane(n, 6));
    equal(4, SIMD.Int8x16.extractLane(n, 7));
    equal(1, SIMD.Int8x16.extractLane(n, 8));
    equal(2, SIMD.Int8x16.extractLane(n, 9));
    equal(3, SIMD.Int8x16.extractLane(n, 10));
    equal(4, SIMD.Int8x16.extractLane(n, 11));
    equal(1, SIMD.Int8x16.extractLane(n, 12));
    equal(2, SIMD.Int8x16.extractLane(n, 13));
    equal(3, SIMD.Int8x16.extractLane(n, 14));
    equal(4, SIMD.Int8x16.extractLane(n, 15));
}

function testWithY() {
    print("Int8x16 replaceLane 1");
    var a = SIMD.Int8x16(16, 9, 4, 1, 16, 9, 4, 1, 16, 9, 4, 1, 16, 9, 4, 1);
    var c = SIMD.Int8x16.replaceLane(a, 1, 20);
    equal(16, SIMD.Int8x16.extractLane(c, 0));
    equal(20, SIMD.Int8x16.extractLane(c, 1));
    equal(4, SIMD.Int8x16.extractLane(c, 2));
    equal(1, SIMD.Int8x16.extractLane(c, 3));
    equal(16, SIMD.Int8x16.extractLane(c, 4));
    equal(9, SIMD.Int8x16.extractLane(c, 5));
    equal(4, SIMD.Int8x16.extractLane(c, 6));
    equal(1, SIMD.Int8x16.extractLane(c, 7));
    equal(16, SIMD.Int8x16.extractLane(c, 8));
    equal(9, SIMD.Int8x16.extractLane(c, 9));
    equal(4, SIMD.Int8x16.extractLane(c, 10));
    equal(1, SIMD.Int8x16.extractLane(c, 11));
    equal(16, SIMD.Int8x16.extractLane(c, 12));
    equal(9, SIMD.Int8x16.extractLane(c, 13));
    equal(4, SIMD.Int8x16.extractLane(c, 14));
    equal(1, SIMD.Int8x16.extractLane(c, 15));
    var m = SIMD.Int8x16(1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4);
    var n = SIMD.Int8x16.replaceLane(m, 1, 20);
    equal(1, SIMD.Int8x16.extractLane(n, 0));
    equal(20, SIMD.Int8x16.extractLane(n, 1));
    equal(3, SIMD.Int8x16.extractLane(n, 2));
    equal(4, SIMD.Int8x16.extractLane(n, 3));
    equal(1, SIMD.Int8x16.extractLane(n, 4));
    equal(2, SIMD.Int8x16.extractLane(n, 5));
    equal(3, SIMD.Int8x16.extractLane(n, 6));
    equal(4, SIMD.Int8x16.extractLane(n, 7));
    equal(1, SIMD.Int8x16.extractLane(n, 8));
    equal(2, SIMD.Int8x16.extractLane(n, 9));
    equal(3, SIMD.Int8x16.extractLane(n, 10));
    equal(4, SIMD.Int8x16.extractLane(n, 11));
    equal(1, SIMD.Int8x16.extractLane(n, 12));
    equal(2, SIMD.Int8x16.extractLane(n, 13));
    equal(3, SIMD.Int8x16.extractLane(n, 14));
    equal(4, SIMD.Int8x16.extractLane(n, 15));
}


function testWithZ() {
    print("Int8x16 replaceLane 2");
    var a = SIMD.Int8x16(16, 9, 4, 1, 16, 9, 4, 1, 16, 9, 4, 1, 16, 9, 4, 1);
    var c = SIMD.Int8x16.replaceLane(a, 2, 20);
    equal(16, SIMD.Int8x16.extractLane(c, 0));
    equal(9, SIMD.Int8x16.extractLane(c, 1));
    equal(20, SIMD.Int8x16.extractLane(c, 2));
    equal(1, SIMD.Int8x16.extractLane(c, 3));
    equal(16, SIMD.Int8x16.extractLane(c, 4));
    equal(9, SIMD.Int8x16.extractLane(c, 5));
    equal(4, SIMD.Int8x16.extractLane(c, 6));
    equal(1, SIMD.Int8x16.extractLane(c, 7));
    equal(16, SIMD.Int8x16.extractLane(c, 8));
    equal(9, SIMD.Int8x16.extractLane(c, 9));
    equal(4, SIMD.Int8x16.extractLane(c, 10));
    equal(1, SIMD.Int8x16.extractLane(c, 11));
    equal(16, SIMD.Int8x16.extractLane(c, 12));
    equal(9, SIMD.Int8x16.extractLane(c, 13));
    equal(4, SIMD.Int8x16.extractLane(c, 14));
    equal(1, SIMD.Int8x16.extractLane(c, 15));
    var m = SIMD.Int8x16(1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4);
    var n = SIMD.Int8x16.replaceLane(m, 2, 20);
    equal(1, SIMD.Int8x16.extractLane(n, 0));
    equal(2, SIMD.Int8x16.extractLane(n, 1));
    equal(20, SIMD.Int8x16.extractLane(n, 2));
    equal(4, SIMD.Int8x16.extractLane(n, 3));
    equal(1, SIMD.Int8x16.extractLane(n, 4));
    equal(2, SIMD.Int8x16.extractLane(n, 5));
    equal(3, SIMD.Int8x16.extractLane(n, 6));
    equal(4, SIMD.Int8x16.extractLane(n, 7));
    equal(1, SIMD.Int8x16.extractLane(n, 8));
    equal(2, SIMD.Int8x16.extractLane(n, 9));
    equal(3, SIMD.Int8x16.extractLane(n, 10));
    equal(4, SIMD.Int8x16.extractLane(n, 11));
    equal(1, SIMD.Int8x16.extractLane(n, 12));
    equal(2, SIMD.Int8x16.extractLane(n, 13));
    equal(3, SIMD.Int8x16.extractLane(n, 14));
    equal(4, SIMD.Int8x16.extractLane(n, 15));
}

function testWithW() {
    print("Int8x16 replaceLane 3");
    var a = SIMD.Int8x16(16, 9, 4, 1, 16, 9, 4, 1, 16, 9, 4, 1, 16, 9, 4, 1);
    var c = SIMD.Int8x16.replaceLane(a, 3, 20);
    equal(16, SIMD.Int8x16.extractLane(c, 0));
    equal(9, SIMD.Int8x16.extractLane(c, 1));
    equal(4, SIMD.Int8x16.extractLane(c, 2));
    equal(20, SIMD.Int8x16.extractLane(c, 3));
    equal(16, SIMD.Int8x16.extractLane(c, 4));
    equal(9, SIMD.Int8x16.extractLane(c, 5));
    equal(4, SIMD.Int8x16.extractLane(c, 6));
    equal(1, SIMD.Int8x16.extractLane(c, 7));
    equal(16, SIMD.Int8x16.extractLane(c, 8));
    equal(9, SIMD.Int8x16.extractLane(c, 9));
    equal(4, SIMD.Int8x16.extractLane(c, 10));
    equal(1, SIMD.Int8x16.extractLane(c, 11));
    equal(16, SIMD.Int8x16.extractLane(c, 12));
    equal(9, SIMD.Int8x16.extractLane(c, 13));
    equal(4, SIMD.Int8x16.extractLane(c, 14));
    equal(1, SIMD.Int8x16.extractLane(c, 15));
    var m = SIMD.Int8x16(1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4);
    var n = SIMD.Int8x16.replaceLane(m, 8, 20);
    equal(1, SIMD.Int8x16.extractLane(n, 0));
    equal(2, SIMD.Int8x16.extractLane(n, 1));
    equal(3, SIMD.Int8x16.extractLane(n, 2));
    equal(20, SIMD.Int8x16.extractLane(n, 8));
    equal(1, SIMD.Int8x16.extractLane(n, 4));
    equal(2, SIMD.Int8x16.extractLane(n, 5));
    equal(3, SIMD.Int8x16.extractLane(n, 6));
    equal(4, SIMD.Int8x16.extractLane(n, 7));
    equal(20, SIMD.Int8x16.extractLane(n, 8));
    equal(2, SIMD.Int8x16.extractLane(n, 9));
    equal(3, SIMD.Int8x16.extractLane(n, 10));
    equal(4, SIMD.Int8x16.extractLane(n, 11));
    equal(1, SIMD.Int8x16.extractLane(n, 12));
    equal(2, SIMD.Int8x16.extractLane(n, 13));
    equal(3, SIMD.Int8x16.extractLane(n, 14));
    equal(4, SIMD.Int8x16.extractLane(n, 15));
}

function testReplaceLane() {
    print("Int8x16 replaceLane 4-15");
    var a = SIMD.Int8x16(16, 9, 4, 1, 16, 9, 4, 1, 16, 9, 4, 1, 16, 9, 4, 1);
    for (var i = 4; i < 16; ++i)
    {
        var c = SIMD.Int8x16.replaceLane(a, i, 20);
        equal(20, SIMD.Int8x16.extractLane(c, i));
        for(var j = 0; j < 16; ++j)
        {
            if(j != i)
            {
                equal(SIMD.Int8x16.extractLane(a, j), SIMD.Int8x16.extractLane(c, j));
            }
        }
    }
}

testWithX();
testWithX();
testWithX();
testWithX();
testWithX();
testWithX();
testWithX();
testWithX();

testWithY();
testWithY();
testWithY();
testWithY();
testWithY();
testWithY();
testWithY();
testWithY();

testWithZ();
testWithZ();
testWithZ();
testWithZ();
testWithZ();
testWithZ();
testWithZ();
testWithZ();

testWithW();
testWithW();
testWithW();
testWithW();
testWithW();
testWithW();
testWithW();
testWithW();

testReplaceLane();