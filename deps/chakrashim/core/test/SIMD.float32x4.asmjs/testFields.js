//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var i4splat = i4.splat;
    var i4fromFloat64x2 = i4.fromFloat64x2;
    var i4fromFloat64x2Bits = i4.fromFloat64x2Bits;
    var i4fromFloat32x4 = i4.fromFloat32x4;
    var i4fromFloat32x4Bits = i4.fromFloat32x4Bits;
    //var i4abs = i4.abs;
    var i4neg = i4.neg;
    var i4add = i4.add;
    var i4sub = i4.sub;
    var i4mul = i4.mul;
    //var i4swizzle = i4.swizzle;
    //var i4shuffle = i4.shuffle;
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
    var f4extractLane = f4.extractLane;
    var f4replaceLane = f4.replaceLane;
    var f4check = f4.check;
    var f4splat = f4.splat;
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
    //var f4swizzle = f4.swizzle;
    //var f4shuffle = f4.shuffle;
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

    var f4g1 = f4(-5033.2, -3401.0, 665.34, 32234.1);          // global var initialized
    var f4g2 = f4(1194580.33, -11201.5, 63236.93, 334.8);          // global var initialized

    var i4g1 = i4(1065353216, -1073741824, -1077936128, 1082130432);          // global var initialized
    var i4g2 = i4(353216, -492529, -1128, 1085);          // global var initialized

    var gval = 1234;
    var gval2 = 1234.0;

    var sf = f4(0.12, -1.0, 1.14, 2000.002);
    var loopCOUNT = 3;

    function func1(a) {
        a = a | 0;
        var b = f4(5033.2, -3401.0, 665.34, -32234.1);
        var c = f4(-34183.8985, 212344.12, 665.34, 65534.99);
        var d = i4(0, -1, 0, -1);


        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            b = f4(f4extractLane(b, 3) * f4extractLane(b, 2), f4extractLane(b, 2) / f4extractLane(b, 1), f4extractLane(b, 1) + f4extractLane(b, 0), -f4extractLane(b, 0));


            loopIndex = (loopIndex + 1) | 0;
        }

        return f4check(b);
    }

    function func2(a) {
        a = a | 0;
        var b = f4(5033.2, -3401.0, 665.34, -32234.1);
        var c = f4(-34183.8985, 212344.12, -569437.0, 65534.99);
        var d = i4(0, -1, 0, -1);
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {

            b = f4(f4extractLane(f4g1, 3) * f4extractLane(b, 2), f4extractLane(f4g2, 2) / f4extractLane(b, 2), -f4extractLane(f4g1, 1), f4extractLane(f4g2, 0) + fround(1.0));


        }

        return f4check(b);
    }

    function func3(a) {
        a = a | 0;
        var b = f4(5033.2, -3401.0, 665.34, -32234.1);
        var c = f4(-34183.8985, 212344.12, -569437.0, 65534.99);
        var d = i4(0, -1, 0, -1);

        var loopIndex = 0;

        loopIndex = loopCOUNT | 0;
        do {

            globImportF4 = f4(f4extractLane(globImportF4, 0) + fround(2.0), fround(4.0) * f4extractLane(globImportF4, 1), fround(1.0) / f4extractLane(f4g2, 2), f4extractLane(f4g2, 3) * fround(2.0));


            loopIndex = (loopIndex - 1) | 0;
        }
        while ((loopIndex | 0) > 0);

        return f4check(globImportF4);
    }

    function func4(a) {
        a = a | 0;
        var b = f4(5033.2, -3401.0, 665.34, -32234.1);
        var c = f4(-34183.8985, 212344.12, 665.34, 65534.99);
        var d = i4(0, -1, 0, -1);



        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            b = f4(f4extractLane(b, 3) * f4extractLane(b, 2), f4extractLane(b, 2) / f4extractLane(b, 1), f4extractLane(b, 1) + f4extractLane(b, 0), -f4extractLane(b, 0));
            a = b.signMask;


            loopIndex = (loopIndex + 1) | 0;
        }

        return a | 0;
    }

    function func5(a) {
        a = a | 0;
        var b = f4(5033.2, -3401.0, 665.34, -32234.1);
        var c = f4(-34183.8985, 212344.12, -569437.0, 65534.99);
        var d = i4(0, -1, 0, -1);
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0) {

            b = f4(f4extractLane(f4g1, 3) * f4extractLane(b, 2), f4extractLane(f4g2, 3) / f4extractLane(b, 1), -f4extractLane(f4g1, 1), f4extractLane(f4g2, 0) + fround(1.0));
            a = b.signMask;

        }

        return a | 0;
    }

    function func6(a) {
        a = a | 0;
        var b = f4(5033.2, -3401.0, 665.34, -32234.1);
        var c = f4(-34183.8985, 212344.12, -569437.0, 65534.99);
        var d = i4(0, -1, 0, -1);
        var loopIndex = 0;


        loopIndex = loopCOUNT | 0;
        do {

            globImportF4 = f4(f4extractLane(globImportF4, 0) + fround(2.0), fround(4.0) * f4extractLane(globImportF4, 1), fround(1.0) / f4extractLane(f4g2, 2), f4extractLane(f4g2, 3) * fround(2.0));
            a = globImportF4.signMask;

            loopIndex = (loopIndex - 1) | 0;
        }
        while ((loopIndex | 0) > 0);

        return a | 0;
    }

    function func7() {

        var a = fround(0.0);
        var b = fround(0.0);
        var c = fround(0.0);
        var d = fround(0.0);
        a = f4extractLane(sf, 3);
        b = f4extractLane(sf, 2);
        c = f4extractLane(sf, 1);
        d = f4extractLane(sf, 0);
        ////return +d;

        sf = f4replaceLane(sf, 0, fround(fround(89.9) + fround(a)));
        sf = f4replaceLane(sf, 1, fround(b));
        sf = f4replaceLane(sf, 2, fround(c));
        sf = f4replaceLane(sf, 3, fround(f4extractLane(sf, 0)));


        return f4check(sf);
    }


    return { func1: func1, func2: func2, func3: func3, func4: func4, func5: func5, func6: func6, func7: func7 };
}

var m = asmModule(this, { g1: SIMD.Float32x4(90934.2, 123.9, 419.39, 449.0), g2: SIMD.Int32x4(-1065353216, -1073741824, -1077936128, -1082130432) });

var ret;

ret = m.func1();
equalSimd([-459958181167104.000000,2570.530518,-8223534.000000,8215190.500000], ret, SIMD.Float32x4, "func1");


ret = m.func2();
equalSimd([109628176.000000,18.593628,3401.000000,1194581.375000], ret, SIMD.Float32x4, "func2");


ret = m.func3();
equalSimd([90940.203125,7929.600098,0.00001581354445079342,669.599976], ret, SIMD.Float32x4, "func3");

print("Func4");
ret = m.func4();
print(typeof (ret));
print(ret.toString());

print("Func5");
ret = m.func5();
print(typeof (ret));
print(ret.toString());

print("Func6");
ret = m.func6();
print(typeof (ret));
print(ret.toString());

ret = m.func7();
equalSimd([2089.901855,1.140000,-1.000000,2089.901855], ret, SIMD.Float32x4, "func7");