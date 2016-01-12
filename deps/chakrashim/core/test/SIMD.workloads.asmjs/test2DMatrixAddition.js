//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function asmModule(stdlib, imports, buffer) {
    "use asm";

    var log = stdlib.Math.log;
    var toF = stdlib.Math.fround;
    var imul = stdlib.Math.imul;

    var i4 = stdlib.SIMD.Int32x4;
    var i4store = i4.store;
    var i4swizzle = i4.swizzle;
    var i4check = i4.check;

    var f4 = stdlib.SIMD.Float32x4;
    var f4equal = f4.equal;
    var f4lessThan = f4.lessThan;
    var f4splat = f4.splat;
    var f4store = f4.store;
    var f4load = f4.load;
    var f4check = f4.check;
    var f4abs = f4.abs;
    var f4add = f4.add;
    var f4sub = f4.sub;

    var Float32Heap = new stdlib.Float32Array(buffer);
    var Int32Heap = new stdlib.Int32Array(buffer);
    var BLOCK_SIZE = 4;

    function matrixAddition(aIndex, bIndex, cIndex) {
        aIndex = aIndex | 0;
        bIndex = bIndex | 0;
        cIndex = cIndex | 0;

        var i = 0, dim1 = 0, dim2 = 0, matrixSize = 0;
        var aPiece = f4(0.0, 0.0, 0.0, 0.0), bPiece = f4(0.0, 0.0, 0.0, 0.0);

        dim1 = Int32Heap[aIndex << 2 >> 2] | 0;
        dim2 = Int32Heap[aIndex + 1 << 2 >> 2] | 0;
        matrixSize = imul(dim1, dim2);

        //array dimensions don't match
        if (((dim2 | 0) != (Int32Heap[bIndex + 1 << 2 >> 2] | 0)) | ((dim1 | 0) != (Int32Heap[bIndex << 2 >> 2] | 0))) {
            return -1;
        }

        Int32Heap[cIndex << 2 >> 2] = dim1;
        Int32Heap[cIndex + 1 << 2 >> 2] = dim2;

        while ((i|0) < (matrixSize|0)) {
            aPiece = f4load(Float32Heap, aIndex + 2 + i << 2 >> 2);
            bPiece = f4load(Float32Heap, bIndex + 2 + i << 2 >> 2);
            f4store(Float32Heap, cIndex + 2 + i << 2 >> 2, f4add(aPiece, bPiece));

            i = (i + BLOCK_SIZE)|0;
        }

        return 0;
    }

    function new2DMatrix(startIndex, dim1, dim2) {
        startIndex = startIndex | 0;
        dim1 = dim1 | 0;
        dim2 = dim2 | 0;

        var i = 0, matrixSize = 0;
        matrixSize = imul(dim1, dim2);
        Int32Heap[startIndex << 2 >> 2] = dim1;
        Int32Heap[startIndex + 1 << 2 >> 2] = dim2;
        for (i = 0; (i|0) < ((matrixSize - BLOCK_SIZE)|0); i = (i + BLOCK_SIZE)|0) {
            f4store(Float32Heap, startIndex + 2 + i << 2 >> 2, f4(toF((i + 1)|0), toF((i + 2)|0), toF((i + 3)|0), toF((i + 4)|0)));
        }
        for (; (i|0) < (matrixSize|0); i = (i + 1)|0) {
            Float32Heap[(startIndex + 2 + i) << 2 >> 2] = toF((i + 1)|0);
        }
        return (startIndex + 2 + i) | 0;
    }

    return {
        new2DMatrix: new2DMatrix,
        matrixAddition: matrixAddition
    };
}

function print2DMatrix(buffer, start) {
    var IntHeap32 = new Int32Array(buffer);
    var FloatHeap32 = new Float32Array(buffer);
    var f4;
    var dim1 = IntHeap32[start];
    var dim2 = IntHeap32[start + 1];
    print(dim1 + " by " + dim2 + " matrix");

    for (var i = 0; i < Math.imul(dim1, dim2) ; i += 4) {
        f4 = SIMD.Float32x4.load(FloatHeap32, i + start + 2);
        print(f4.toString());
    }
}

var buffer = new ArrayBuffer(16 * 1024 * 1024);
var m = asmModule(this, null, buffer);

print("2D Matrix Addition");
m.new2DMatrix(0, 18, 12);
m.new2DMatrix(500, 18, 12);
m.matrixAddition(0, 500, 1000);
print2DMatrix(buffer, 1000);