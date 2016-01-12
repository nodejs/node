//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");

function asmModule(stdlib, imports, buffer) {
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
    var i4load  = i4.load;
    var i4load1 = i4.load1;
    var i4load2 = i4.load2;
    var i4load3 = i4.load3;

    var i4store  = i4.store
    var i4store1 = i4.store1;
    var i4store2 = i4.store2;
    var i4store3 = i4.store3;

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

    var f4load = f4.load;
    var f4load1 = f4.load1;
    var f4load2 = f4.load2;
    var f4load3 = f4.load3;

    var f4store  = f4.store;
    var f4store1 = f4.store1;
    var f4store2 = f4.store2;
    var f4store3 = f4.store3;

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
    var d2swizzle = d2.swizzle;
    var d2shuffle = d2.shuffle;
    var d2lessThan = d2.lessThan;
    var d2lessThanOrEqual = d2.lessThanOrEqual;
    var d2equal = d2.equal;
    var d2notEqual = d2.notEqual;
    var d2greaterThan = d2.greaterThan;
    var d2greaterThanOrEqual = d2.greaterThanOrEqual;
    var d2select = d2.select;

    var d2load  = d2.load;
    var d2load1 = d2.load1;

    var d2store  = d2.store
    var d2store1 = d2.store1;

    var fround = stdlib.Math.fround;

    var globImportF4 = f4check(imports.g1);       // global var import
    var globImportI4 = i4check(imports.g2);       // global var import
    var globImportD2 = d2check(imports.g3);       // global var import
    var g1 = f4(-5033.2,-3401.0,665.34,32234.1);          // global var initialized
    var g2 = i4(1065353216, -1073741824, -1077936128, 1082130432);          // global var initialized
    var g3 = d2(0.12344,-1.6578);          // global var initialized
    var gval = 1234;
    var gval2 = 1234.0;

    var OFFSET_1 = 10;
    var OFFSET_2 = 15;

    var loopCOUNT = 10;

    var Int8Heap = new stdlib.Int8Array (buffer);
    var Uint8Heap = new stdlib.Uint8Array (buffer);

    var Int16Heap = new stdlib.Int16Array(buffer);
    var Uint16Heap = new stdlib.Uint16Array(buffer);
    var Int32Heap = new stdlib.Int32Array(buffer);
    var Uint32Heap = new stdlib.Uint32Array(buffer);
    var Float32Heap = new stdlib.Float32Array(buffer);

    // Merges count number of SIMD vectors from srcIdx1 and srcIdx2 to dstIdx
    function merge(srcIdx1, srcIdx2, dstIdx, count)
    {
        srcIdx1 = srcIdx1 | 0;
        srcIdx2 = srcIdx2 | 0;
        dstIdx = dstIdx | 0;
        count = count | 0;

        var i = 0;

        for (; (i|0) < ((2 * count)|0); i = (i + 1) |0)
        {
            if ((((i | 0) % 2)|0) == 0)
            {
                f4store(Int8Heap, dstIdx, f4load(Int8Heap, srcIdx1));
                //f4store(Int8Heap, (dstIdx + 0), f4load(Int8Heap, (srcIdx1 + 0)));
                srcIdx1 = (srcIdx1 + 16) | 0;

            }
            else
            {
                f4store(Int8Heap, dstIdx, f4load(Int8Heap, srcIdx2));
                //f4store(Int8Heap, (dstIdx + 0), f4load(Int8Heap, (srcIdx1 + 0)));
                srcIdx2 = (srcIdx2 + 16) | 0;
            }
            dstIdx = (dstIdx + 16) | 0;
        }
        return -1;
    }

    function merge1(srcIdx1, srcIdx2, dstIdx, count)
    {
        srcIdx1 = srcIdx1 | 0;
        srcIdx2 = srcIdx2 | 0;
        dstIdx = dstIdx | 0;
        count = count | 0;

        var i = 0;

        for (; (i|0) < ((2 * count)|0); i = (i + 1) |0)
        {
            if ((((i | 0) % 2)|0) == 0)
            {
                i4store(Int8Heap, dstIdx, i4load(Int8Heap, srcIdx1));
                //f4store(Int8Heap, (dstIdx + 0), f4load(Int8Heap, (srcIdx1 + 0)));
                srcIdx1 = (srcIdx1 + 16) | 0;

            }
            else
            {
                i4store(Int8Heap, dstIdx, i4load(Int8Heap, srcIdx2));
                //f4store(Int8Heap, (dstIdx + 0), f4load(Int8Heap, (srcIdx1 + 0)));
                srcIdx2 = (srcIdx2 + 16) | 0;
            }
            dstIdx = (dstIdx + 16) | 0;
        }

        return -1;
    }
    function merge2(srcIdx1, srcIdx2, dstIdx, count)
    {
        srcIdx1 = srcIdx1 | 0;
        srcIdx2 = srcIdx2 | 0;
        dstIdx = dstIdx | 0;
        count = count | 0;

        var i = 0;

        for (; (i|0) < ((2 * count)|0); i = (i + 1) |0)
        {
            if ((((i | 0) % 2)|0) == 0)
            {
                d2store(Int8Heap, dstIdx, d2load(Int8Heap, srcIdx1));
                //f4store(Int8Heap, (dstIdx + 0), f4load(Int8Heap, (srcIdx1 + 0)));
                srcIdx1 = (srcIdx1 + 16) | 0;

            }
            else
            {
                d2store(Int8Heap, dstIdx, d2load(Int8Heap, srcIdx2));
                //f4store(Int8Heap, (dstIdx + 0), f4load(Int8Heap, (srcIdx1 + 0)));
                srcIdx2 = (srcIdx2 + 16) | 0;
            }
            dstIdx = (dstIdx + 16) | 0;
        }

        return -1;
    }
    return {merge:merge
            , merge1:merge1
            , merge2:merge2};
}

var buffer = new ArrayBuffer(0x10000); //16mb min 2^12

//Reset or flush the buffer
function initF32(buffer) {
    var values = new Float32Array( buffer );
    for( var i=0; i < values.length ; ++i ) {
        values[i] = i * 10;
    }
    return values.length;
}
function initI32(buffer) {
    var values = new Int32Array( buffer );
    for( var i=0; i < values.length ; ++i ) {
        values[i] = i * 10;
    }
    return values.length;
}
function validateBuffer(buffer, count)
{
    var f4;
    var data = [
    SIMD.Float32x4(0.0,10.0,20.0,30.0),
    SIMD.Float32x4(40.0,50.0,60.0,70.0),
    SIMD.Float32x4(80.0,90.0,100.0,110.0),
    SIMD.Float32x4(120.0,130.0,140.0,150.0),
    SIMD.Float32x4(160.0,170.0,180.0,190.0),
    SIMD.Float32x4(200.0,210.0,220.0,230.0),
    SIMD.Float32x4(240.0,250.0,260.0,270.0),
    SIMD.Float32x4(280.0,290.0,300.0,310.0),
    SIMD.Float32x4(320.0,330.0,340.0,350.0),
    SIMD.Float32x4(360.0,370.0,380.0,390.0),
    SIMD.Float32x4(0.0,10.0,20.0,30.0),
    SIMD.Float32x4(200.0,210.0,220.0,230.0),
    SIMD.Float32x4(40.0,50.0,60.0,70.0),
    SIMD.Float32x4(240.0,250.0,260.0,270.0),
    SIMD.Float32x4(80.0,90.0,100.0,110.0),
    SIMD.Float32x4(280.0,290.0,300.0,310.0),
    SIMD.Float32x4(120.0,130.0,140.0,150.0),
    SIMD.Float32x4(320.0,330.0,340.0,350.0),
    SIMD.Float32x4(160.0,170.0,180.0,190.0),
    SIMD.Float32x4(360.0,370.0,380.0,390.0),
    SIMD.Float32x4(800.0,810.0,820.0,830.0),
    SIMD.Float32x4(840.0,850.0,860.0,870.0),
    SIMD.Float32x4(880.0,890.0,900.0,910.0),
    SIMD.Float32x4(920.0,930.0,940.0,950.0),
    SIMD.Float32x4(960.0,970.0,980.0,990.0)
    ];
    for (var i = 0; i < count/* * 16*/; i += 4)
    {
        f4 = SIMD.Float32x4.load(buffer, i);
           equalSimd(data[i/4], f4, SIMD.Float32x4, "validateBuffer");
    }
}
function validateBuffer1(buffer, count)
{
    var i4;
        var data = [
    SIMD.Int32x4(0,10,20,30),
    SIMD.Int32x4(40,50,60,70),
    SIMD.Int32x4(80,90,100,110),
    SIMD.Int32x4(120,130,140,150),
    SIMD.Int32x4(160,170,180,190),
    SIMD.Int32x4(200,210,220,230),
    SIMD.Int32x4(240,250,260,270),
    SIMD.Int32x4(280,290,300,310),
    SIMD.Int32x4(320,330,340,350),
    SIMD.Int32x4(360,370,380,390),
    SIMD.Int32x4(0,10,20,30),
    SIMD.Int32x4(200,210,220,230),
    SIMD.Int32x4(40,50,60,70),
    SIMD.Int32x4(240,250,260,270),
    SIMD.Int32x4(80,90,100,110),
    SIMD.Int32x4(280,290,300,310),
    SIMD.Int32x4(120,130,140,150),
    SIMD.Int32x4(320,330,340,350),
    SIMD.Int32x4(160,170,180,190),
    SIMD.Int32x4(360,370,380,390),
    SIMD.Int32x4(800,810,820,830),
    SIMD.Int32x4(840,850,860,870),
    SIMD.Int32x4(880,890,900,910),
    SIMD.Int32x4(920,930,940,950),
    SIMD.Int32x4(960,970,980,990)
    ];
    for (var i = 0; i < count/* * 16*/; i += 4)
    {
        i4 = SIMD.Int32x4.load(buffer, i);
        equalSimd(data[i/4], i4, SIMD.Int32x4, "validateBuffer1");
    }
}


inputLength = initF32(buffer);

//Module initialization
var m = asmModule(this, {g0:initF32(buffer),g1:SIMD.Float32x4(9,9,9,9), g2:SIMD.Int32x4(1, 2, 3, 4), g3:SIMD.Float64x2(10, 10, 10, 10)}, buffer);
var values = new Float32Array(buffer);

//Resetting the buffer.
initF32(buffer);

var ret = m.merge(0, 5 * 16, 10 * 16, 5);
validateBuffer(values, 4 * 25);

initI32(buffer);
var ret = m.merge1(0, 5 * 16, 10 * 16, 5);
validateBuffer1(values, 4 * 25);

WScript.Echo("PASS");