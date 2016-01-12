//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

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
    var Float64Heap = new stdlib.Float64Array(buffer);

    //Reverses heap values at start index and end index
    function reverseI4(start, end)
    {
        start = start | 0;
        end = end | 0;

        var BLOCK_SIZE = 4;
        var ret1 = i4(0, 0, 0, 0), ret2 = i4(0, 0, 0, 0);
        var i = 0, temp = 0;

        while(((BLOCK_SIZE * 2)|0) <= (((end - start) - ((i * 2)|0))|0))
        {
            ret1 = i4swizzle(i4load(Int32Heap, (start + i) << 2 >> 2), 3, 2, 1, 0);
            ret2 = i4swizzle(i4load(Int32Heap, (end - i - BLOCK_SIZE) << 2 >> 2), 3, 2, 1, 0);

            i4store(Int32Heap, (end - i - BLOCK_SIZE) << 2 >> 2, ret1);
            i4store(Int32Heap, (start + i) << 2 >> 2, ret2);

            i = (i + BLOCK_SIZE)|0;
        }

        while(1 < ((((end - start) + 0) - ((i * 2)|0))|0))
        {
            temp = Int32Heap[start + i << 2 >> 2] | 0;
            Int32Heap[start + i << 2 >> 2] = Int32Heap[(end - i - 1) << 2 >> 2];
            Int32Heap[(end - i - 1) << 2 >> 2] = temp;
            i = (i + 1)|0;
        }
    }
    //Reverses heap values at start index and end index
    function reverseF4(start, end)
    {
        start = start | 0;
        end = end | 0;

        var BLOCK_SIZE = 4;
        var ret1 = f4(0.0, 0.0, 0.0, 0.0), ret2 = f4(0.0, 0.0, 0.0, 0.0);
        var i = 0, temp = 0;

        while(((BLOCK_SIZE * 2)|0) <= (((end - start) - ((i * 2)|0))|0))
        {
            ret1 = f4swizzle(f4load(Int32Heap, (start + i) << 2 >> 2), 3, 2, 1, 0);
            ret2 = f4swizzle(f4load(Int32Heap, (end - i - BLOCK_SIZE) << 2 >> 2), 3, 2, 1, 0);

            f4store(Int32Heap, (end - i - BLOCK_SIZE) << 2 >> 2, ret1);
            f4store(Int32Heap, (start + i) << 2 >> 2, ret2);

            i = (i + BLOCK_SIZE)|0;
        }

        while(1 < ((((end - start) + 0) - ((i * 2)|0))|0))
        {
            temp = Int32Heap[start + i << 2 >> 2] | 0;
            Int32Heap[start + i << 2 >> 2] = Int32Heap[(end - i - 1) << 2 >> 2];
            Int32Heap[(end - i - 1) << 2 >> 2] = temp;
            i = (i + 1)|0;
        }
    }

    //Reverses heap values at start index and end index
    function reverseD2(start, end)
    {
        start = start | 0;
        end = end | 0;

        var BLOCK_SIZE = 4;
        var ret1 = d2(0.0, 0.0), ret2 = d2(0.0, 0.0);
        var start32 = 0, end32 = 0;
        var i = 0, i64 = 0, temp = 0.0;

        start32 = (start * 2)|0;
        end32 = (end * 2)|0;
        while(((BLOCK_SIZE * 2)|0) <= (((end32 - start32) - ((i * 2)|0))|0))
        {
            ret1 = d2swizzle(d2load(Float32Heap, (start32 + i) << 2 >> 2), 1, 0);
            ret2 = d2swizzle(d2load(Float32Heap, (end32 - i - BLOCK_SIZE) << 2 >> 2), 1, 0);

            d2store(Float32Heap, (end32 - i - BLOCK_SIZE) << 2 >> 2, ret1);
            d2store(Float32Heap, (start32 + i) << 2 >> 2, ret2);

            i = (i + BLOCK_SIZE)|0;
            i64 = (i64 + 2)|0;
        }

        while(1 < ((((end - start) + 0) - ((i64 * 2)|0))|0))
        {
            temp = +Float64Heap[start + i64 << 3 >> 3];
            Float64Heap[start + i64 << 3 >> 3] = Float64Heap[(end - i64 - 1) << 3 >> 3];
            Float64Heap[(end - i64 - 1) << 3 >> 3] = temp;
            i64 = (i64 + 1)|0;
        }
    }

    return {reverseI4:reverseI4, reverseF4:reverseF4, reverseD2:reverseD2};
}

var buffer = new ArrayBuffer(0x10000); //16mb min 2^12

//Reset or flush the buffer
function initI4(buffer) {
    var values = new Int32Array( buffer );
    for( var i=0; i < values.length ; ++i ) {
        values[i] = i * 10;
    }
    return values.length;
}

//Reset or flush the buffer
function initF4(buffer) {
    var values = new Float32Array( buffer );
    for( var i=0; i < values.length ; ++i ) {
        values[i] = i * 10;
    }
    return values.length;
}

//Reset or flush the buffer
function initD2(buffer) {
    var values = new Float64Array( buffer );
    for( var i=0; i < values.length ; ++i ) {
        values[i] = i * 10;
    }
    return values.length;
}
function printBufferD2(buffer, count)
{
    var d2;
    for (var i = 0; i < count/* * 16*/; i += 4)
    {
        d2 = SIMD.Float64x2.load(buffer, i);
        print(d2.toString());
    }
}

function printBufferI4(buffer, count)
{
    var i4;
    for (var i = 0; i < count/* * 16*/; i += 4)
    {
        i4 = SIMD.Int32x4.load(buffer, i);
        print(i4.toString());
    }
}

function printBufferF4(buffer, count)
{
    var f4;
    for (var i = 0; i < count/* * 16*/; i += 4)
    {
        f4 = SIMD.Float32x4.load(buffer, i);
        print(f4.toString());
    }
}

function printResults(res)
{
    print(typeof(res));
    print(res.toString());
}

//Module initialization
var m = asmModule(this, {g0:initI4(buffer),g1:SIMD.Float32x4(9,9,9,9), g2:SIMD.Int32x4(1, 2, 3, 4), g3:SIMD.Float64x2(10, 10, 10, 10)}, buffer);
var values = new Float32Array(buffer);

//Resetting the buffer.
initI4(buffer);

print("Reversing - Start");
printBufferI4(values, 8 * 4);
m.reverseI4(3, 31);
print("Reversing - Start");
printBufferI4(values, 8 * 4);

//Resetting the buffer.
initF4(buffer);

print("Reversing - Start");
printBufferF4(values, 8 * 4);
m.reverseF4(3, 31);
print("Reversing - Start");
printBufferF4(values, 8 * 4);

//Resetting the buffer.
initD2(buffer);

print("Reversing - Start");
printBufferD2(values, 10 * 2);
m.reverseD2(3, 8);
print("Reversing - Start");
printBufferD2(values, 10 * 2);
