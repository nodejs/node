//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

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

    var d2 = stdlib.SIMD.Float64x2;  
    var d2check = d2.check;
    var d2splat = d2.splat;
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
    //var d2swizzle = d2.swizzle;
    //var d2shuffle = d2.shuffle;
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
    var globImportD2 = d2check(imports.g3);       // global var import
    var g1 = f4(-5033.2,-3401.0,665.34,32234.1);          // global var initialized
    var g2 = i4(1065353216, -1073741824, -1077936128, 1082130432);          // global var initialized
    var g3 = d2(0.12344,-1.6578);          // global var initialized
    var gval = 1234;
    var gval2 = 1234.0;


    
    var loopCOUNT = 3;

    function func1(a, b)
    {
        a = i4check(a);
        b = i4check(b);
        var x = i4(0,0,0,0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = i4add(a, b);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(x);
    }
    
    function func2(a, b, c, d)
    {
        a = i4check(a);
        b = i4check(b);
        c = i4check(c);
        d = i4check(d);
        var x = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = i4check(func1(a, b));
            y = i4check(func1(c, d));
            

        }

        return i4check(i4add(x,y));
    }

    function func3(a, b, c, d, e, f, g, h)
    {
        a = i4check(a);
        b = i4check(b);
        c = i4check(c);
        d = i4check(d);
        e = i4check(e);
        f = i4check(f);
        g = i4check(g);
        h = i4check(h);        
        
        var x = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = i4check(func2(a, b, c, d));
            y = i4check(func2(e, f, g, h));
            
        }

        return i4check(i4add(x,y));
    }

    function func4() { //Testing for a bug while returning SIMD values from a loop
        var value1 = i4(0, 0, 0, 0);
        var i = 0;

        for (i = 0; (i | 0) < 1000; i = (i + 1)|0) {
            value1 = i4add(value1, i4splat(1));
            if ((i | 0) == 300) {
                return i4check(value1);
            }
        }
    }
    
    // TODO: Test conversion of returned value
    function value()
    {
        var ret = 1.0;
        var i = 1.0;


        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            ret = ret + i;

            loopIndex = (loopIndex + 1) | 0;
        }

        return +ret;
    }
    
    return {func1:func1, func2:func2, func3:func3, func4:func4/*, func5:func5, func6:func6*/};
}

var m = asmModule(this, {g1:SIMD.Float32x4(90934.2,123.9,419.39,449.0), g2:SIMD.Int32x4(-1065353216, -1073741824,-1077936128, -1082130432), g3:SIMD.Float64x2(110.20, 58967.0, 14511.670, 191766.23431)});

var s1 = SIMD.Int32x4(1.0, 2.0, 3.0, 4.0);
var s2 = SIMD.Int32x4(1.0, 2.0, 3.0, 4.0);
var s3 = SIMD.Int32x4(1.0, 2.0, 3.0, 4.0);
var s4 = SIMD.Int32x4(1.0, 2.0, 3.0, 4.0);
var s5 = SIMD.Int32x4(1.0, 2.0, 3.0, 4.0);
var s6 = SIMD.Int32x4(1.0, 2.0, 3.0, 4.0);
var s7 = SIMD.Int32x4(1.0, 2.0, 3.0, 4.0);
var s8 = SIMD.Int32x4(1.0, 2.0, 3.0, 4.0);

var ret1 = m.func1(s1, s2);
var ret2 = m.func2(s1, s2, s3, s4);
var ret3 = m.func3(s1, s2, s3, s4, s5, s6, s7, s8);
var ret4 = m.func4();

/*
var ret5 = m.func5();
var ret6 = m.func6();
*/
/*
var ret7 = m.func7();
var ret8 = m.func8();
var ret9 = m.func9();


var ret10 = m.func10();
var ret11 = m.func11();
var ret12 = m.func12();

*/

print(typeof(ret1));
print(ret1.toString());

print(typeof(ret2));
print(ret2.toString());

print(typeof(ret3));
print(ret3.toString());

print(typeof(ret4));
print(ret4.toString());


/*
print(typeof(ret5));
print(ret5.toString());

print(typeof(ret6));
print(ret6.toString());
*/
/*
print(typeof(ret7));
print(ret7.toString());

print(typeof(ret8));
print(ret8.toString());

print(typeof(ret9));
print(ret9.toString());

print(typeof(ret10));
print(ret10.toString());

print(typeof(ret11));
print(ret11.toString());

print(typeof(ret12));
print(ret12.toString());
*/
