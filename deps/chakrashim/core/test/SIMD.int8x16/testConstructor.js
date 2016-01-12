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

function testConstructor() {
    print("Constructor");
    equal(SIMD.Int8x16, undefined);
    equal(SIMD.Int8x16(1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4), undefined);
    var a = SIMD.Int8x16("2014/10/10", -0, 127, 126, "2014/10/10", -0, 127, 126, "2014/10/10", -0, 127, 126, "2014/10/10", -0, 127, 126);
    print("a.1: " + SIMD.Int8x16.extractLane(a, 0));
    print("a.2: " + SIMD.Int8x16.extractLane(a, 1));
    print("a.3: " + SIMD.Int8x16.extractLane(a, 2));
    print("a.4: " + SIMD.Int8x16.extractLane(a, 3));
    print("a.5: " + SIMD.Int8x16.extractLane(a, 4));
    print("a.6: " + SIMD.Int8x16.extractLane(a, 5));
    print("a.7: " + SIMD.Int8x16.extractLane(a, 6));
    print("a.8: " + SIMD.Int8x16.extractLane(a, 7));
    print("a.9: " + SIMD.Int8x16.extractLane(a, 8));
    print("a.10: " + SIMD.Int8x16.extractLane(a, 9));
    print("a.11: " + SIMD.Int8x16.extractLane(a, 10));
    print("a.12: " + SIMD.Int8x16.extractLane(a, 11));
    print("a.13: " + SIMD.Int8x16.extractLane(a, 12));
    print("a.14: " + SIMD.Int8x16.extractLane(a, 13));
    print("a.15: " + SIMD.Int8x16.extractLane(a, 14));
    print("a.16: " + SIMD.Int8x16.extractLane(a, 15));

    var b = SIMD.Int8x16(4, 3, 2, 1, 4, 3, 2, 1, 4, 3, 2, 1, 4, 3, 2, 1);
    var c = SIMD.Int8x16.check(b);
    equal(c, b);
    equal(SIMD.Int8x16.extractLane(c, 0), SIMD.Int8x16.extractLane(b, 0));
    equal(SIMD.Int8x16.extractLane(c, 1), SIMD.Int8x16.extractLane(b, 1));
    equal(SIMD.Int8x16.extractLane(c, 2), SIMD.Int8x16.extractLane(b, 2));
    equal(SIMD.Int8x16.extractLane(c, 3), SIMD.Int8x16.extractLane(b, 3));
    equal(SIMD.Int8x16.extractLane(c, 4), SIMD.Int8x16.extractLane(b, 4));
    equal(SIMD.Int8x16.extractLane(c, 5), SIMD.Int8x16.extractLane(b, 5));
    equal(SIMD.Int8x16.extractLane(c, 6), SIMD.Int8x16.extractLane(b, 6));
    equal(SIMD.Int8x16.extractLane(c, 7), SIMD.Int8x16.extractLane(b, 7));
    equal(SIMD.Int8x16.extractLane(c, 8), SIMD.Int8x16.extractLane(b, 8));
    equal(SIMD.Int8x16.extractLane(c, 9), SIMD.Int8x16.extractLane(b, 9));
    equal(SIMD.Int8x16.extractLane(c, 10), SIMD.Int8x16.extractLane(b, 10));
    equal(SIMD.Int8x16.extractLane(c, 11), SIMD.Int8x16.extractLane(b, 11));
    equal(SIMD.Int8x16.extractLane(c, 12), SIMD.Int8x16.extractLane(b, 12));
    equal(SIMD.Int8x16.extractLane(c, 13), SIMD.Int8x16.extractLane(b, 13));
    equal(SIMD.Int8x16.extractLane(c, 14), SIMD.Int8x16.extractLane(b, 14));
    equal(SIMD.Int8x16.extractLane(c, 15), SIMD.Int8x16.extractLane(b, 15));

    try {
        var m = SIMD.Int8x16.check(1)
    }
    catch (e) {
        print("Type Error");
    }
}
function testFromFloat32x4Bits() {
    var m = SIMD.float32x4.fromInt8x16Bits(SIMD.Int8x16(0x3F800000, 0x40000000, 0x40400000, 0x40800000));
    var n = SIMD.Int8x16.fromFloat32x4Bits(m);
    print("FromFloat32x4Bits");
    equal(1, n.x);
    equal(2, n.y);
    equal(3, n.z);
    equal(4, n.w);
    var a = SIMD.float32x4(1.0, 2.0, 3.0, 4.0);
    var b = SIMD.Int8x16.fromFloat32x4Bits(a);
    equal(0x3F800000, b.x);
    equal(0x40000000, b.y);
    equal(0x40400000, b.z);
    equal(0x40800000, b.w);
}

testConstructor();
testConstructor();
testConstructor();
testConstructor();
testConstructor();
testConstructor();