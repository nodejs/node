//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function InitBuffer1(buffer) {
    HEAP32 = new Float32Array(buffer);
    HEAP32[0] = 13654.67
    HEAP32[1] = 23.0
    HEAP32[2] = 13654.565
    HEAP32[3] = 13654.111
    HEAP32[4] = 423.00006
    HEAP32[5] = 4.0
    HEAP32[6] = 5.56
    HEAP32[7] = 3.56
    HEAP32[8] = 145764.56756
    HEAP32[9] = 53.57
}

function InitBuffer2(buffer) {
    HEAP32 = new Float32Array(buffer);
    for (var i = 0.0; i < 999999.0 - 1; i = i + 1.0) {
        HEAP32[i] = i + 1.0;
    }
}

function asmModule(stdlib, imports, buffer) {
    "use asm";

    var log = stdlib.Math.log;
    var toF = stdlib.Math.fround;

    var i4 = stdlib.SIMD.Int32x4;
    var i4swizzle = i4.swizzle;
    var i4check = i4.check;

    var f4 = stdlib.SIMD.Float32x4;
    var f4equal = f4.equal;
    var f4lessThan = f4.lessThan;
    var f4splat = f4.splat;
    var f4load = f4.load;
    var f4check = f4.check;
    var f4abs = f4.abs;
    var f4sub = f4.sub;

    var HEAP32 = new stdlib.Float32Array(buffer);
    var BLOCK_SIZE = 4;
    var i = 0;

    function linearSearch(value, length) {
        value = toF(value);
        length = length | 0;
        var f4Value = f4(0.0, 0.0, 0.0, 0.0);
        var f4Heap = f4(0.0, 0.0, 0.0, 0.0);
        var i4Result = i4(0, 0, 0, 0);
        var i4Flipped = i4(0, 0, 0, 0);

        f4Value = f4splat(value);

        for (i = 0; (i | 0) < (length|0); i = (i + BLOCK_SIZE) | 0) {
            //f4Heap = f4(HEAP32[(i << 2) >> 2], HEAP32[((i + 1) << 2) >> 2], HEAP32[((i + 2) << 2) >> 2], HEAP32[((i + 3) << 2) >> 2]);
            f4Heap = f4load(HEAP32, (i << 2) >> 2);
            i4Result = i4check(f4nearlyEqual(f4Heap, f4Value, toF(0.0001)));

            if (i4Result.signMask != 0) {
                i4Flipped = i4swizzle(i4Result, 3, 2, 1, 0);
                return (i + BLOCK_SIZE - ~~(log(+(i4Flipped.signMask | 0)) / log(2.0)) - 1) | 0
            }
        }
        return -1;
    }

    function f4nearlyEqual(a, b, epsilon) {
        a = f4check(a);
        b = f4check(b);
        epsilon = toF(epsilon);

        var diff = f4(0.0, 0.0, 0.0, 0.0);

        diff = f4abs(f4sub(a, b));
        return f4lessThan(diff, f4splat(epsilon));
    }

    return { linearSearch: linearSearch };
}

var buffer = new ArrayBuffer(16 * 1024 * 1024);
var m = asmModule(this, null, buffer);

InitBuffer1(buffer);
print("List 1");
print(m.linearSearch(13654.67, 10));
print(m.linearSearch(23.0, 10));
print(m.linearSearch(145764.56756, 10));
print(m.linearSearch(53.57, 10));
print(m.linearSearch(-53, 10));

InitBuffer2(buffer);
print("List 2");
print(m.linearSearch(13654.0, 999999));
print(m.linearSearch(23.0, 999999));
print(m.linearSearch(145764.0, 999999));
print(m.linearSearch(-53.0, 999999));