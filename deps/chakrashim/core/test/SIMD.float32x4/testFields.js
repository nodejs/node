//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function equal(a, b) {
    if (a == b)
        print("Correct")
    else
        print(">> Fail!")
}
var sf = SIMD.Float32x4(1.35, -2.0, 3.4, 0.0);
function testExtractLane() {
    print("F4 ExtractLane");

    print(typeof sf);
    print(sf.toString());

    print(typeof SIMD.Float32x4.extractLane(sf, 0));
    print(SIMD.Float32x4.extractLane(sf, 0).toString());

    print(typeof SIMD.Float32x4.extractLane(sf, 1))
    print(SIMD.Float32x4.extractLane(sf, 1).toString());

    print(typeof SIMD.Float32x4.extractLane(sf, 2));
    print(SIMD.Float32x4.extractLane(sf, 2).toString());

    print(typeof SIMD.Float32x4.extractLane(sf, 3));
    print(SIMD.Float32x4.extractLane(sf, 3).toString());
}

function testReplaceLane() {
    print("F4 ReplaceLane");

    print(typeof sf);
    print(sf.toString());

    var v = SIMD.Float32x4.replaceLane(sf, 0, 10.2)
    print(typeof v);
    print(v.toString());

    v = SIMD.Float32x4.replaceLane(sf, 1, 12.3)
    print(typeof v);
    print(v.toString());

    v = SIMD.Float32x4.replaceLane(sf, 2, -30.2)
    print(typeof v);
    print(v.toString());

    v = SIMD.Float32x4.replaceLane(sf, 3, 0.0)
    print(typeof v);
    print(v.toString());

}

function testScalarGetters() {
    print('Float32x4 scalar getters');
    var a = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);
    equal(1.0, SIMD.Float32x4.extractLane(a, 0));
    equal(2.0, SIMD.Float32x4.extractLane(a, 1));
    equal(3.0, SIMD.Float32x4.extractLane(a, 2));
    equal(4.0, SIMD.Float32x4.extractLane(a, 3));
}

function testSignMask() {
    print('Float32x4 signMask');
    var a = SIMD.Float32x4(-1.0, -2.0, -3.0, -4.0);
    equal(0xf, a.signMask);
    var b = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);
    equal(0x0, b.signMask);
    var c = SIMD.Float32x4(1.0, -2.0, -3.0, 4.0);
    equal(0x6, c.signMask);
    var d = SIMD.Float32x4(-0.0, 0.0, 0.0, -0.0);
    equal(0x9, d.signMask);
    var e = SIMD.Float32x4(0.0, -0.0, -0.0, 0.0);
    equal(0x6, e.signMask);
}

function testVectorGetters() {
    print('Float32x4 vector getters');
    var a = SIMD.Float32x4(4.0, 3.0, 2.0, 1.0);
    var xxxx = SIMD.Float32x4.shuffle(a, SIMD.XXXX);
    var yyyy = SIMD.Float32x4.shuffle(a, SIMD.YYYY);
    var zzzz = SIMD.Float32x4.shuffle(a, SIMD.ZZZZ);
    var wwww = SIMD.Float32x4.shuffle(a, SIMD.WWWW);
    var wzyx = SIMD.Float32x4.shuffle(a, SIMD.WZYX);
    equal(4.0, SIMD.Float32x4.extractLane(xxxx, 0));
    equal(4.0, SIMD.Float32x4.extractLane(xxxx, 1));
    equal(4.0, SIMD.Float32x4.extractLane(xxxx, 2));
    equal(4.0, SIMD.Float32x4.extractLane(xxxx, 3));
    equal(3.0, SIMD.Float32x4.extractLane(yyyy, 0));
    equal(3.0, SIMD.Float32x4.extractLane(yyyy, 1));
    equal(3.0, SIMD.Float32x4.extractLane(yyyy, 2));
    equal(3.0, SIMD.Float32x4.extractLane(yyyy, 3));
    equal(2.0, SIMD.Float32x4.extractLane(zzzz, 0));
    equal(2.0, SIMD.Float32x4.extractLane(zzzz, 1));
    equal(2.0, SIMD.Float32x4.extractLane(zzzz, 2));
    equal(2.0, SIMD.Float32x4.extractLane(zzzz, 3));
    equal(1.0, SIMD.Float32x4.extractLane(wwww, 0));
    equal(1.0, SIMD.Float32x4.extractLane(wwww, 1));
    equal(1.0, SIMD.Float32x4.extractLane(wwww, 2));
    equal(1.0, SIMD.Float32x4.extractLane(wwww, 3));
    equal(1.0, SIMD.Float32x4.extractLane(wzyx, 0));
    equal(2.0, SIMD.Float32x4.extractLane(wzyx, 1));
    equal(3.0, SIMD.Float32x4.extractLane(wzyx, 2));
    equal(4.0, SIMD.Float32x4.extractLane(wzyx, 3));
}

testScalarGetters();
testScalarGetters();
testScalarGetters();
testScalarGetters();
testScalarGetters();
testScalarGetters();
testScalarGetters();
testScalarGetters();

testExtractLane();
print();
testReplaceLane();
print();