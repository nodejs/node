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

function testInt32x4BitConversion() {
    print("Float32x4 Int32x4 bit conversion");
    var m = SIMD.Int32x4(0x3F800000, 0x40000000, 0x40400000, 0x40800000);
    var n = SIMD.Float32x4.fromInt32x4Bits(m);
    equal(1.0, SIMD.Float32x4.extractLane(n, 0));
    equal(2.0, SIMD.Float32x4.extractLane(n, 1));
    equal(3.0, SIMD.Float32x4.extractLane(n, 2));
    equal(4.0, SIMD.Float32x4.extractLane(n, 3));
    n = SIMD.Float32x4(5.0, 6.0, 7.0, 8.0);
    m = SIMD.Int32x4.fromFloat32x4Bits(n);
    equal(0x40A00000, SIMD.Int32x4.extractLane(m, 0));
    equal(0x40C00000, SIMD.Int32x4.extractLane(m, 1));
    equal(0x40E00000, SIMD.Int32x4.extractLane(m, 2));
    equal(0x41000000, SIMD.Int32x4.extractLane(m, 3));
    // Flip sign using bit-wise operators.
    n = SIMD.Float32x4(9.0, 10.0, 11.0, 12.0);
    m = SIMD.Int32x4(0x80000000, 0x80000000, 0x80000000, 0x80000000);
    var nMask = SIMD.Int32x4.fromFloat32x4Bits(n);
    nMask = SIMD.Int32x4.xor(nMask, m); // flip sign.
    n = SIMD.Float32x4.fromInt32x4Bits(nMask);
    equal(-9.0, SIMD.Float32x4.extractLane(n, 0));
    equal(-10.0, SIMD.Float32x4.extractLane(n, 1));
    equal(-11.0, SIMD.Float32x4.extractLane(n, 2));
    equal(-12.0, SIMD.Float32x4.extractLane(n, 3));
    nMask = SIMD.Int32x4.fromFloat32x4Bits(n);
    nMask = SIMD.Int32x4.xor(nMask, m); // flip sign.
    n = SIMD.Float32x4.fromInt32x4Bits(nMask);
    equal(9.0, SIMD.Float32x4.extractLane(n, 0));
    equal(10.0, SIMD.Float32x4.extractLane(n, 1));
    equal(11.0, SIMD.Float32x4.extractLane(n, 2));
    equal(12.0, SIMD.Float32x4.extractLane(n, 3));
    // Should stay unmodified across bit conversions
    m = SIMD.Int32x4(0xFFFFFFFF, 0xFFFF0000, 0x80000000, 0x0);
    var m2 = SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.fromInt32x4Bits(m));
    equal(SIMD.Int32x4.extractLane(m, 0), SIMD.Int32x4.extractLane(m2, 0));
    equal(SIMD.Int32x4.extractLane(m, 1), SIMD.Int32x4.extractLane(m2, 1));
    equal(SIMD.Int32x4.extractLane(m, 2), SIMD.Int32x4.extractLane(m2, 2));
    equal(SIMD.Int32x4.extractLane(m, 3), SIMD.Int32x4.extractLane(m2, 3));
}

testInt32x4BitConversion();
testInt32x4BitConversion();
testInt32x4BitConversion();
testInt32x4BitConversion();
testInt32x4BitConversion();
testInt32x4BitConversion();
testInt32x4BitConversion();