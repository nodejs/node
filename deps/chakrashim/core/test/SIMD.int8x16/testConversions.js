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

function testInt32x4BitConversion() {
    print("Int8x16 Int32x4 bit conversion");
    var m = SIMD.Int32x4(0x3F800000, 0x40000000, 0x40400000, 0x40800000);
    var n = SIMD.Int8x16.fromInt32x4Bits(m);
    equal(0, SIMD.Int8x16.extractLane(n, 0));
    equal(0, SIMD.Int8x16.extractLane(n, 1));
    equal(-128, SIMD.Int8x16.extractLane(n, 2));
    equal(63, SIMD.Int8x16.extractLane(n, 3));
    equal(0, SIMD.Int8x16.extractLane(n, 4));
    equal(0, SIMD.Int8x16.extractLane(n, 5));
    equal(0, SIMD.Int8x16.extractLane(n, 6));
    equal(64, SIMD.Int8x16.extractLane(n, 7));
    equal(0, SIMD.Int8x16.extractLane(n, 8));
    equal(0, SIMD.Int8x16.extractLane(n, 9));
    equal(64, SIMD.Int8x16.extractLane(n, 10));
    equal(64, SIMD.Int8x16.extractLane(n, 11));
    equal(0, SIMD.Int8x16.extractLane(n, 12));
    equal(0, SIMD.Int8x16.extractLane(n, 13));
    equal(-128, SIMD.Int8x16.extractLane(n, 14));
    equal(64, SIMD.Int8x16.extractLane(n, 15));
    n = SIMD.Int32x4(5, 6, 7, 8);
    m = SIMD.Int8x16.fromInt32x4Bits(n);
    equal(0x05, SIMD.Int8x16.extractLane(m, 0));
    equal(0x00, SIMD.Int8x16.extractLane(m, 1));
    equal(0x00, SIMD.Int8x16.extractLane(m, 2));
    equal(0x00, SIMD.Int8x16.extractLane(m, 3));
    equal(0x06, SIMD.Int8x16.extractLane(m, 4));
    equal(0x00, SIMD.Int8x16.extractLane(m, 5));
    equal(0x00, SIMD.Int8x16.extractLane(m, 6));
    equal(0x00, SIMD.Int8x16.extractLane(m, 7));
    equal(0x07, SIMD.Int8x16.extractLane(m, 8));
    equal(0x00, SIMD.Int8x16.extractLane(m, 9));
    equal(0x00, SIMD.Int8x16.extractLane(m, 10));
    equal(0x00, SIMD.Int8x16.extractLane(m, 11));
    equal(0x08, SIMD.Int8x16.extractLane(m, 12));
    equal(0x00, SIMD.Int8x16.extractLane(m, 13));
    equal(0x00, SIMD.Int8x16.extractLane(m, 14));
    equal(0x00, SIMD.Int8x16.extractLane(m, 15));
    // Flip sign using bit-wise operators.
    n = SIMD.Int32x4(9, 10, 11, 12);
    m = SIMD.Int8x16(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
    var nMask = SIMD.Int8x16.fromInt32x4Bits(n);
    nMask = SIMD.Int8x16.xor(nMask, m); // flip sign.

    equal(-119, SIMD.Int8x16.extractLane(nMask, 0));
    equal(-128, SIMD.Int8x16.extractLane(nMask, 1));
    equal(-128, SIMD.Int8x16.extractLane(nMask, 2));
    equal(-128, SIMD.Int8x16.extractLane(nMask, 3));
    equal(-118, SIMD.Int8x16.extractLane(nMask, 4));
    equal(-128, SIMD.Int8x16.extractLane(nMask, 5));
    equal(-128, SIMD.Int8x16.extractLane(nMask, 6));
    equal(-128, SIMD.Int8x16.extractLane(nMask, 7));
    equal(-117, SIMD.Int8x16.extractLane(nMask, 8));
    equal(-128, SIMD.Int8x16.extractLane(nMask, 9));
    equal(-128, SIMD.Int8x16.extractLane(nMask, 10));
    equal(-128, SIMD.Int8x16.extractLane(nMask, 11));
    equal(-116, SIMD.Int8x16.extractLane(nMask, 12));
    equal(-128, SIMD.Int8x16.extractLane(nMask, 13));
    equal(-128, SIMD.Int8x16.extractLane(nMask, 14));
    equal(-128, SIMD.Int8x16.extractLane(nMask, 15));
}

function testFloat32x4BitsConversion() {
    print("Int8x16 Float32x4 bits conversion");
    var m = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);
    var n = SIMD.Int8x16.fromFloat32x4Bits(m);
    equal(0, SIMD.Int8x16.extractLane(n, 0));
    equal(0, SIMD.Int8x16.extractLane(n, 1));
    equal(-128, SIMD.Int8x16.extractLane(n, 2));
    equal(63, SIMD.Int8x16.extractLane(n, 3));
    equal(0, SIMD.Int8x16.extractLane(n, 4));
    equal(0, SIMD.Int8x16.extractLane(n, 5));
    equal(0, SIMD.Int8x16.extractLane(n, 6));
    equal(64, SIMD.Int8x16.extractLane(n, 7));
    equal(0, SIMD.Int8x16.extractLane(n, 8));
    equal(0, SIMD.Int8x16.extractLane(n, 9));
    equal(64, SIMD.Int8x16.extractLane(n, 10));
    equal(64, SIMD.Int8x16.extractLane(n, 11));
    equal(0, SIMD.Int8x16.extractLane(n, 12));
    equal(0, SIMD.Int8x16.extractLane(n, 13));
    equal(-128, SIMD.Int8x16.extractLane(n, 14));
    equal(64, SIMD.Int8x16.extractLane(n, 15));

    var a = SIMD.Float32x4(0x90128012, 0x84121012, 0xF0128432, 0x10001012);
    var b = SIMD.Int8x16.fromFloat32x4Bits(a);
    equal(-128, SIMD.Int8x16.extractLane(b, 0));
    equal(18, SIMD.Int8x16.extractLane(b, 1));
    equal(16, SIMD.Int8x16.extractLane(b, 2));
    equal(79, SIMD.Int8x16.extractLane(b, 3));
    equal(16, SIMD.Int8x16.extractLane(b, 4));
    equal(18, SIMD.Int8x16.extractLane(b, 5));
    equal(4, SIMD.Int8x16.extractLane(b, 6));
    equal(79, SIMD.Int8x16.extractLane(b, 7));
    equal(-124, SIMD.Int8x16.extractLane(b, 8));
    equal(18, SIMD.Int8x16.extractLane(b, 9));
    equal(112, SIMD.Int8x16.extractLane(b, 10));
    equal(79, SIMD.Int8x16.extractLane(b, 11));
    equal(-127, SIMD.Int8x16.extractLane(b, 12));
    equal(0, SIMD.Int8x16.extractLane(b, 13));
    equal(-128, SIMD.Int8x16.extractLane(b, 14));
    equal(77, SIMD.Int8x16.extractLane(b, 15));
}

testInt32x4BitConversion();
testInt32x4BitConversion();
testInt32x4BitConversion();
testInt32x4BitConversion();
testInt32x4BitConversion();
testInt32x4BitConversion();
testInt32x4BitConversion();

testFloat32x4BitsConversion();
testFloat32x4BitsConversion();
testFloat32x4BitsConversion();
testFloat32x4BitsConversion();
testFloat32x4BitsConversion();
testFloat32x4BitsConversion();
testFloat32x4BitsConversion();
testFloat32x4BitsConversion();