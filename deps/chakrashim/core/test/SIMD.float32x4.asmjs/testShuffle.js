//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function asmModule(stdlib, imports) {
    "use asm";

    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
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
    var f4extractLane = f4.extractLane;

    var f4select = f4.select;
    var f4and = f4.and;
    var f4or = f4.or;
    var f4xor = f4.xor;
    var f4not = f4.not;

    var d2 = stdlib.SIMD.Float64x2;
    var d2check = d2.check;
    var d2fromFloat32x4 = d2.fromFloat32x4;
    var d2fromFloat32x4Bits = d2.fromFloat32x4Bits;
    var d2fromInt32x4 = d2.fromInt32x4;
    var d2fromInt32x4Bits = d2.fromInt32x4Bits;
    var d2abs = d2.abs;
    var d2neg = d2.neg;
    var d2add = d2.add;
    var d2sub = d2.sub;
    var d2mul = d2.mul;
    var d2div = d2.div;
    var d2clamp = d2.clamp;
    var d2min = d2.min;
    var d2max = d2.max;
    var d2reciprocal = d2.reciprocal;
    var d2reciprocalSqrt = d2.reciprocalSqrt;
    var d2sqrt = d2.sqrt;
    var d2swizzle = d2.swizzle;
    var d2shuffle = d2.shuffle;
    var d2lessThan = d2.lessThan;
    var d2lessThanOrEqual = d2.lessThanOrEqual;
    var d2equal = d2.equal;
    var d2notEqual = d2.notEqual;
    var d2greaterThan = d2.greaterThan;
    var d2greaterThanOrEqual = d2.greaterThanOrEqual;
    var d2select = d2.select;

    var fround = stdlib.Math.fround;

    var globImportF4 = f4check(imports.g1);       // global var import
    var globImportI4 = i4check(imports.g2);       // global var import
    var g1 = f4(1.0, 2.0, 3.0, -0.0);          // global var initialized
    var g2 = f4(-5.3, -0.0, 7.332, 8.0);          // global var initialized
    var g3 = i4(1, 2, 3, 4);          // global var initialized
    var g4 = i4(5, 6, 7, 8);          // global var initialized
    var g5 = d2(1.0, 2.0);          // global var initialized
    var g6 = d2(3.0, 4.0);          // global var initialized
    var gval = 1234;
    var gval2 = 1234.0;

    var f4splat = f4.splat;

    var sqrt = stdlib.Math.sqrt;
    var pow = stdlib.Math.pow;

    var loopCOUNT = 3;

    function shuffle1() {
        var xyxy = f4(0.0, 0.0, 0.0, 0.0);
        var zwzw = f4(0.0, 0.0, 0.0, 0.0);
        var xxxx = f4(0.0, 0.0, 0.0, 0.0);
        var xxyy = f4(0.0, 0.0, 0.0, 0.0);

        var x = 0.0, y = 0.0, z = 0.0, w = 0.0;

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            xyxy = f4shuffle(g1, g2, 0, 1, 4, 5);
            zwzw = f4shuffle(g1, g2, 2, 3, 6, 7);
            xxxx = f4shuffle(g1, g2, 0, 0, 4, 4);
            xxyy = f4shuffle(g1, g2, 0, 0, 5, 5);

            loopIndex = (loopIndex + 1) | 0;
        }
     
        x = +sqrt((+fround(f4extractLane(xyxy, 0)) * +fround(f4extractLane(xyxy, 0)))
                + (+fround(f4extractLane(xyxy, 1)) * +fround(f4extractLane(xyxy, 1)))
                + (+fround(f4extractLane(xyxy, 2)) * +fround(f4extractLane(xyxy, 2)))
                + (+fround(f4extractLane(xyxy, 3)) * +fround(f4extractLane(xyxy, 3))));
        y = +sqrt((+fround(f4extractLane(zwzw, 0)) * +fround(f4extractLane(zwzw, 0)))
                + (+fround(f4extractLane(zwzw, 1)) * +fround(f4extractLane(zwzw, 1)))
                + (+fround(f4extractLane(zwzw, 2)) * +fround(f4extractLane(zwzw, 2)))
                + (+fround(f4extractLane(zwzw, 3)) * +fround(f4extractLane(zwzw, 3))));
        z = +sqrt((+fround(f4extractLane(xxxx, 0)) * +fround(f4extractLane(xxxx, 0)))
                + (+fround(f4extractLane(xxxx, 1)) * +fround(f4extractLane(xxxx, 1)))
                + (+fround(f4extractLane(xxxx, 2)) * +fround(f4extractLane(xxxx, 2)))
                + (+fround(f4extractLane(xxxx, 3)) * +fround(f4extractLane(xxxx, 3))));
        w = +sqrt((+fround(f4extractLane(xxyy, 0)) * +fround(f4extractLane(xxyy, 0)))
                + (+fround(f4extractLane(xxyy, 1)) * +fround(f4extractLane(xxyy, 1)))
                + (+fround(f4extractLane(xxyy, 2)) * +fround(f4extractLane(xxyy, 2)))
                + (+fround(f4extractLane(xxyy, 3)) * +fround(f4extractLane(xxyy, 3))));

        return f4check(f4(fround(x), fround(y), fround(z), fround(w)));

    }

    function shuffle2() {
        var xyxy = f4(0.0, 0.0, 0.0, 0.0);
        var zwzw = f4(0.0, 0.0, 0.0, 0.0);
        var xxxx = f4(0.0, 0.0, 0.0, 0.0);
        var xxyy = f4(0.0, 0.0, 0.0, 0.0);
        var v1 = f4(122.0, -0.0, 334.0, -9500.231);
        var v2 = f4(102.0, -33.13, -1.0, 233.000001);
        var x = 0.0, y = 0.0, z = 0.0, w = 0.0;

        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {
            xyxy = f4shuffle(v1, v2, 0, 1, 4, 5);
            zwzw = f4shuffle(v1, v2, 2, 3, 6, 7);

            xxxx = f4shuffle(v1, v2, 0, 0, 4, 4);
            xxyy = f4shuffle(v1, v2, 0, 0, 5, 5);
        }

    x = +sqrt((+fround(f4extractLane(xyxy, 0)) * +fround(f4extractLane(xyxy, 0)))
            + (+fround(f4extractLane(xyxy, 1)) * +fround(f4extractLane(xyxy, 1)))
            + (+fround(f4extractLane(xyxy, 2)) * +fround(f4extractLane(xyxy, 2)))
            + (+fround(f4extractLane(xyxy, 3)) * +fround(f4extractLane(xyxy, 3))));
    y = +sqrt((+fround(f4extractLane(zwzw, 0)) * +fround(f4extractLane(zwzw, 0)))
            + (+fround(f4extractLane(zwzw, 1)) * +fround(f4extractLane(zwzw, 1)))
            + (+fround(f4extractLane(zwzw, 2)) * +fround(f4extractLane(zwzw, 2)))
            + (+fround(f4extractLane(zwzw, 3)) * +fround(f4extractLane(zwzw, 3))));
    z = +sqrt((+fround(f4extractLane(xxxx, 0)) * +fround(f4extractLane(xxxx, 0)))
            + (+fround(f4extractLane(xxxx, 1)) * +fround(f4extractLane(xxxx, 1)))
            + (+fround(f4extractLane(xxxx, 2)) * +fround(f4extractLane(xxxx, 2)))
            + (+fround(f4extractLane(xxxx, 3)) * +fround(f4extractLane(xxxx, 3))));
    w = +sqrt((+fround(f4extractLane(xxyy, 0)) * +fround(f4extractLane(xxyy, 0)))
            + (+fround(f4extractLane(xxyy, 1)) * +fround(f4extractLane(xxyy, 1)))
            + (+fround(f4extractLane(xxyy, 2)) * +fround(f4extractLane(xxyy, 2)))
            + (+fround(f4extractLane(xxyy, 3)) * +fround(f4extractLane(xxyy, 3))));

        return f4check(f4(fround(x), fround(y), fround(z), fround(w)));

    }

    function shuffle3() {
        var xyxy = f4(0.0, 0.0, 0.0, 0.0);
        var zwzw = f4(0.0, 0.0, 0.0, 0.0);
        var xxxx = f4(0.0, 0.0, 0.0, 0.0);
        var xxyy = f4(0.0, 0.0, 0.0, 0.0);

        var x = 0.0, y = 0.0, z = 0.0, w = 0.0;
        var v1 = f4(122.0, -0.0, 334.0, -9500.231);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            xyxy = f4shuffle(f4add(v1, g2), v1, 0, 1, 4, 5);
            zwzw = f4shuffle(f4mul(v1, g2), g1, 2, 3, 6, 7);
            xxxx = f4shuffle(f4sub(v1, g2), v1, 0, 0, 4, 4);
            xxyy = f4shuffle(g2, v1, 0, 0, 5, 5);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ((loopIndex | 0) > 0);

    x = +sqrt((+fround(f4extractLane(xyxy, 0)) * +fround(f4extractLane(xyxy, 0)))
            + (+fround(f4extractLane(xyxy, 1)) * +fround(f4extractLane(xyxy, 1)))
            + (+fround(f4extractLane(xyxy, 2)) * +fround(f4extractLane(xyxy, 2)))
            + (+fround(f4extractLane(xyxy, 3)) * +fround(f4extractLane(xyxy, 3))));
    y = +sqrt((+fround(f4extractLane(zwzw, 0)) * +fround(f4extractLane(zwzw, 0)))
            + (+fround(f4extractLane(zwzw, 1)) * +fround(f4extractLane(zwzw, 1)))
            + (+fround(f4extractLane(zwzw, 2)) * +fround(f4extractLane(zwzw, 2)))
            + (+fround(f4extractLane(zwzw, 3)) * +fround(f4extractLane(zwzw, 3))));
    z = +sqrt((+fround(f4extractLane(xxxx, 0)) * +fround(f4extractLane(xxxx, 0)))
            + (+fround(f4extractLane(xxxx, 1)) * +fround(f4extractLane(xxxx, 1)))
            + (+fround(f4extractLane(xxxx, 2)) * +fround(f4extractLane(xxxx, 2)))
            + (+fround(f4extractLane(xxxx, 3)) * +fround(f4extractLane(xxxx, 3))));
    w = +sqrt((+fround(f4extractLane(xxyy, 0)) * +fround(f4extractLane(xxyy, 0)))
            + (+fround(f4extractLane(xxyy, 1)) * +fround(f4extractLane(xxyy, 1)))
            + (+fround(f4extractLane(xxyy, 2)) * +fround(f4extractLane(xxyy, 2)))
            + (+fround(f4extractLane(xxyy, 3)) * +fround(f4extractLane(xxyy, 3))));
        return f4check(f4(fround(x), fround(y), fround(z), fround(w)));
    }

    return { func1: shuffle1, func2: shuffle2, func3: shuffle3 };
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