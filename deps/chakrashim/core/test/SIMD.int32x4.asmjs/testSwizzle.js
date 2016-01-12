//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function asmModule(stdlib, imports) {
    "use asm";

    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var i4extractLane = i4.extractLane;
    var i4fromFloat64x2 = i4.fromFloat64x2;
    var i4fromFloat64x2Bits = i4.fromFloat64x2Bits;
    var i4fromFloat32x4 = i4.fromFloat32x4;
    var i4fromFloat32x4Bits = i4.fromFloat32x4Bits;
    //var i4abs = i4.abs;
    var i4neg = i4.neg;
    var i4add = i4.add;
    var i4sub = i4.sub;
    var i4mul = i4.mul;
    var i4swizzle = i4.swizzle;
    var i4shuffle = i4.shuffle;
    var i4lessThan = i4.lessThan;
    var i4equal = i4.equal;
    var i4greaterThan = i4.greaterThan;
    var i4select = i4.select;
    var i4and = i4.and;
    var i4or = i4.or;
    var i4xor = i4.xor;
    var i4not = i4.not;
    //var i4shiftLeftByScalar = i4.shiftLeftByScalar;
    //var i4shiftRightByScalar = i4.shiftRightByScalar;
    //var i4shiftRightArithmeticByScalar = i4.shiftRightArithmeticByScalar;

    var f4 = stdlib.SIMD.Float32x4;
    var f4check = f4.check;
    var f4fromFloat64x2 = f4.fromFloat64x2;
    var f4fromFloat64x2Bits = f4.fromFloat64x2Bits;
    var f4fromInt32x4 = f4.fromInt32x4;
    var f4fromInt32x4Bits = f4.fromInt32x4Bits;
    var f4abs = f4.abs;
    var f4neg = f4.neg;
    var f4add = f4.add;
    var f4sub = f4.sub;
    var f4mul = f4.mul;
    var f4div = f4.div;
    var f4clamp = f4.clamp;
    var f4min = f4.min;
    var f4max = f4.max;
    var f4reciprocal = f4.reciprocal;
    var f4reciprocalSqrt = f4.reciprocalSqrt;
    var f4sqrt = f4.sqrt;
    var f4swizzle = f4.swizzle;
    var f4shuffle = f4.shuffle;
    var f4lessThan = f4.lessThan;
    var f4lessThanOrEqual = f4.lessThanOrEqual;
    var f4equal = f4.equal;
    var f4notEqual = f4.notEqual;
    var f4greaterThan = f4.greaterThan;
    var f4greaterThanOrEqual = f4.greaterThanOrEqual;

    var f4select = f4.select;
    var f4and = f4.and;
    var f4or = f4.or;
    var f4xor = f4.xor;
    var f4not = f4.not;


    var fround = stdlib.Math.fround;

    var globImportF4 = f4check(imports.g1);       // global var import
    var globImportI4 = i4check(imports.g2);       // global var import
    var g1 = f4(1.0, 2.0, 3.0, -0.0);          // global var initialized
    var g2 = f4(-5.3, -0.0, 7.332, 8.0);          // global var initialized
    var g3 = i4(1, 2, 3, 4);          // global var initialized
    var g4 = i4(5, 6, 7, 8);          // global var initialized
    var gval = 1234;
    var gval2 = 1234.0;

    var f4splat = f4.splat;

    var sqrt = stdlib.Math.sqrt;
    var pow = stdlib.Math.pow;

    var loopCOUNT = 3;

    function swizzle1() {
        var xyxy = i4(0, 0, 0, 0);
        var zwzw = i4(0, 0, 0, 0);
        var xxxx = i4(0, 0, 0, 0);
        var xxyy = i4(0, 0, 0, 0);


        var x = 0, y = 0, z = 0, w = 0;

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            xyxy = i4swizzle(g3, 0, 1, 0, 1);
            zwzw = i4swizzle(g3, 2, 3, 2, 3);
            xxxx = i4swizzle(g3, 0, 0, 0, 0);
            xxyy = i4swizzle(g3, 0, 0, 1, 1);

            loopIndex = (loopIndex + 1) | 0;
        }
        x = (((i4extractLane(xyxy, 0) + i4extractLane(xyxy, 0)) | 0) + ((i4extractLane(xyxy, 1) + i4extractLane(xyxy, 1)) | 0) + ((i4extractLane(xyxy, 2) + i4extractLane(xyxy, 2)) | 0) + ((i4extractLane(xyxy, 3) + i4extractLane(xyxy, 3)) | 0)) | 0;
        y = (((i4extractLane(zwzw, 0) + i4extractLane(zwzw, 0)) | 0) + ((i4extractLane(zwzw, 1) + i4extractLane(zwzw, 1)) | 0) + ((i4extractLane(zwzw, 2) + i4extractLane(zwzw, 2)) | 0) + ((i4extractLane(zwzw, 3) + i4extractLane(zwzw, 3)) | 0)) | 0;
        z = (((i4extractLane(xxxx, 0) + i4extractLane(xxxx, 0)) | 0) + ((i4extractLane(xxxx, 1) + i4extractLane(xxxx, 1)) | 0) + ((i4extractLane(xxxx, 2) + i4extractLane(xxxx, 2)) | 0) + ((i4extractLane(xxxx, 3) + i4extractLane(xxxx, 3)) | 0)) | 0;
        w = (((i4extractLane(xxyy, 0) + i4extractLane(xxyy, 0)) | 0) + ((i4extractLane(xxyy, 1) + i4extractLane(xxyy, 1)) | 0) + ((i4extractLane(xxyy, 2) + i4extractLane(xxyy, 2)) | 0) + ((i4extractLane(xxyy, 3) + i4extractLane(xxyy, 3)) | 0)) | 0;

        return i4check(i4(x, y, z, w));
    }

    function swizzle2() {
        var xyxy = i4(0, 0, 0, 0);
        var zwzw = i4(0, 0, 0, 0);
        var xxxx = i4(0, 0, 0, 0);
        var xxyy = i4(0, 0, 0, 0);
        var v1 = i4(122, 0, 334, -9500);
        var v2 = i4(102, 3313, 1, 233);
        var x = 0, y = 0, z = 0, w = 0;

        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            xyxy = i4swizzle(i4add(v1, v2), 0, 1, 0, 1);
            zwzw = i4swizzle(i4mul(v1, v2), 2, 3, 2, 3);
            xxxx = i4swizzle(i4sub(v1, v2), 0, 0, 0, 0);
            xxyy = i4swizzle(v2, 0, 0, 1, 1);
        }

        x = (((i4extractLane(xyxy, 0) + i4extractLane(xyxy, 0)) | 0) + ((i4extractLane(xyxy, 1) + i4extractLane(xyxy, 1)) | 0) + ((i4extractLane(xyxy, 2) + i4extractLane(xyxy, 2)) | 0) + ((i4extractLane(xyxy, 3) + i4extractLane(xyxy, 3)) | 0)) | 0;
        y = (((i4extractLane(zwzw, 0) + i4extractLane(zwzw, 0)) | 0) + ((i4extractLane(zwzw, 1) + i4extractLane(zwzw, 1)) | 0) + ((i4extractLane(zwzw, 2) + i4extractLane(zwzw, 2)) | 0) + ((i4extractLane(zwzw, 3) + i4extractLane(zwzw, 3)) | 0)) | 0;
        z = (((i4extractLane(xxxx, 0) + i4extractLane(xxxx, 0)) | 0) + ((i4extractLane(xxxx, 1) + i4extractLane(xxxx, 1)) | 0) + ((i4extractLane(xxxx, 2) + i4extractLane(xxxx, 2)) | 0) + ((i4extractLane(xxxx, 3) + i4extractLane(xxxx, 3)) | 0)) | 0;
        w = (((i4extractLane(xxyy, 0) + i4extractLane(xxyy, 0)) | 0) + ((i4extractLane(xxyy, 1) + i4extractLane(xxyy, 1)) | 0) + ((i4extractLane(xxyy, 2) + i4extractLane(xxyy, 2)) | 0) + ((i4extractLane(xxyy, 3) + i4extractLane(xxyy, 3)) | 0)) | 0;

        return i4check(i4(x, y, z, w));
    }

    function swizzle3() {
        var xyxy = i4(0, 0, 0, 0);
        var zwzw = i4(0, 0, 0, 0);
        var xxxx = i4(0, 0, 0, 0);
        var xxyy = i4(0, 0, 0, 0);
        var v1 = i4(122, 0, 334, -9500);
        var x = 0, y = 0, z = 0, w = 0;

        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            xyxy = i4swizzle(i4add(v1, g3), 0, 1, 0, 1);
            zwzw = i4swizzle(i4mul(v1, g3), 2, 3, 2, 3);
            xxxx = i4swizzle(i4sub(v1, g3), 0, 0, 0, 0);
            xxyy = i4swizzle(g4, 0, 0, 1, 1);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ((loopIndex | 0) > 0);

        x = (((i4extractLane(xyxy, 0) + i4extractLane(xyxy, 0)) | 0) + ((i4extractLane(xyxy, 1) + i4extractLane(xyxy, 1)) | 0) + ((i4extractLane(xyxy, 2) + i4extractLane(xyxy, 2)) | 0) + ((i4extractLane(xyxy, 3) + i4extractLane(xyxy, 3)) | 0)) | 0;
        y = (((i4extractLane(zwzw, 0) + i4extractLane(zwzw, 0)) | 0) + ((i4extractLane(zwzw, 1) + i4extractLane(zwzw, 1)) | 0) + ((i4extractLane(zwzw, 2) + i4extractLane(zwzw, 2)) | 0) + ((i4extractLane(zwzw, 3) + i4extractLane(zwzw, 3)) | 0)) | 0;
        z = (((i4extractLane(xxxx, 0) + i4extractLane(xxxx, 0)) | 0) + ((i4extractLane(xxxx, 1) + i4extractLane(xxxx, 1)) | 0) + ((i4extractLane(xxxx, 2) + i4extractLane(xxxx, 2)) | 0) + ((i4extractLane(xxxx, 3) + i4extractLane(xxxx, 3)) | 0)) | 0;
        w = (((i4extractLane(xxyy, 0) + i4extractLane(xxyy, 0)) | 0) + ((i4extractLane(xxyy, 1) + i4extractLane(xxyy, 1)) | 0) + ((i4extractLane(xxyy, 2) + i4extractLane(xxyy, 2)) | 0) + ((i4extractLane(xxyy, 3) + i4extractLane(xxyy, 3)) | 0)) | 0;
        return i4check(i4(x, y, z, w));
    }

    return { func1: swizzle1 , func2: swizzle2, func3: swizzle3 };
}


var m = asmModule(this, { g1: SIMD.Float32x4(9, 9, 9, 9), g2: SIMD.Int32x4(1, 2, 3, 4) });

var ret1 = m.func1();
var ret2 = m.func2();
var ret3 = m.func3();


print(typeof (ret1));
print(ret1.toString());

print(typeof (ret2));
print(ret2.toString());

print(typeof (ret3));
print(ret3.toString());