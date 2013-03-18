// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax

// A regression test for a register allocation bug triggered by
// http://n3emscripten.appspot.com/dragons_asmjs.html. Note that the test is
// very fragile, but without a proper testing infrastructure for the register
// allocator we can't really do much better.
//
// The code below is a slighty modified version of Emscripten-translated C++
// code for the standard textbook algorithm for frustum culling of an object
// with a given bounding box.
//
// The key problem for the underlying bug was a value with a long live range
// which is used often (a context) and a lot of live ranges starting at the same
// point. The bug was that none of the ranges were allowed to be spilled, so the
// allocator was splitting a live range at its start and re-added the very same
// range into the list of unallocated ranges, making no progress.

(function () {
  function __ZNK4Math5plane3dotERKNS_6float4E(i1, i2) {
    i1 = i1 | 0;
    i2 = i2 | 0;
    return +(+HEAPF32[i1 >> 2] * +HEAPF32[i2 >> 2] + +HEAPF32[i1 + 4 >> 2] * +HEAPF32[i2 + 4 >> 2] + +HEAPF32[i1 + 8 >> 2] * +HEAPF32[i2 + 8 >> 2] + +HEAPF32[i1 + 12 >> 2] * +HEAPF32[i2 + 12 >> 2]);
  }

  function __ZNK4Math7frustum8clipmaskERKNS_5pointE(i1, i2) {
    i1 = i1 | 0;
    i2 = i2 | 0;
    var i3 = 0, i4 = 0;
    i3 = i2 | 0;
    i2 = +__ZNK4Math5plane3dotERKNS_6float4E(i1 | 0, i3) > 0.0 & 1;
    i4 = +__ZNK4Math5plane3dotERKNS_6float4E(i1 + 16 | 0, i3) > 0.0 ? i2 | 2 : i2;
    i2 = +__ZNK4Math5plane3dotERKNS_6float4E(i1 + 32 | 0, i3) > 0.0 ? i4 | 4 : i4;
    i4 = +__ZNK4Math5plane3dotERKNS_6float4E(i1 + 48 | 0, i3) > 0.0 ? i2 | 8 : i2;
    i2 = +__ZNK4Math5plane3dotERKNS_6float4E(i1 + 64 | 0, i3) > 0.0 ? i4 | 16 : i4;
    return (+__ZNK4Math5plane3dotERKNS_6float4E(i1 + 80 | 0, i3) > 0.0 ? i2 | 32 : i2) | 0;
  }

  function __ZNK4Math7frustum10clipstatusERKNS_4bboxE(i1, i2) {
    i1 = i1 | 0;
    i2 = i2 | 0;
    var i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, d17 = 0.0, d18 = 0.0, i19 = 0, i20 = 0, i21 = 0, i22 = 0;
    i3 = STACKTOP;
    STACKTOP = STACKTOP + 16 | 0;
    i4 = i3 | 0;
    i5 = i4 | 0;
    HEAPF32[i5 >> 2] = 0.0;
    i6 = i4 + 4 | 0;
    HEAPF32[i6 >> 2] = 0.0;
    i7 = i4 + 8 | 0;
    HEAPF32[i7 >> 2] = 0.0;
    i8 = i4 + 12 | 0;
    HEAPF32[i8 >> 2] = 1.0;
    i9 = i2 | 0;
    i10 = i2 + 4 | 0;
    i11 = i2 + 8 | 0;
    i12 = i2 + 20 | 0;
    i13 = i2 + 16 | 0;
    i14 = i2 + 24 | 0;
    i2 = 65535;
    i15 = 0;
    i16 = 0;
    while (1) {
      if ((i16 | 0) == 1) {
        d17 = +HEAPF32[i12 >> 2];
        d18 = +HEAPF32[i11 >> 2];
        HEAPF32[i5 >> 2] = +HEAPF32[i9 >> 2];
        HEAPF32[i6 >> 2] = d17;
        HEAPF32[i7 >> 2] = d18;
        HEAPF32[i8 >> 2] = 1.0;
      } else if ((i16 | 0) == 4) {
        HEAPF32[i5 >> 2] = +HEAPF32[i13 >> 2];
        HEAPF32[i6 >> 2] = +HEAPF32[i12 >> 2];
        HEAPF32[i7 >> 2] = +HEAPF32[i14 >> 2];
        HEAPF32[i8 >> 2] = 1.0;
      } else if ((i16 | 0) == 6) {
        d18 = +HEAPF32[i10 >> 2];
        d17 = +HEAPF32[i14 >> 2];
        HEAPF32[i5 >> 2] = +HEAPF32[i9 >> 2];
        HEAPF32[i6 >> 2] = d18;
        HEAPF32[i7 >> 2] = d17;
        HEAPF32[i8 >> 2] = 1.0;
      } else if ((i16 | 0) == 5) {
        d17 = +HEAPF32[i12 >> 2];
        d18 = +HEAPF32[i14 >> 2];
        HEAPF32[i5 >> 2] = +HEAPF32[i9 >> 2];
        HEAPF32[i6 >> 2] = d17;
        HEAPF32[i7 >> 2] = d18;
        HEAPF32[i8 >> 2] = 1.0;
      } else if ((i16 | 0) == 3) {
        d18 = +HEAPF32[i10 >> 2];
        d17 = +HEAPF32[i11 >> 2];
        HEAPF32[i5 >> 2] = +HEAPF32[i13 >> 2];
        HEAPF32[i6 >> 2] = d18;
        HEAPF32[i7 >> 2] = d17;
        HEAPF32[i8 >> 2] = 1.0;
      } else if ((i16 | 0) == 0) {
        HEAPF32[i5 >> 2] = +HEAPF32[i9 >> 2];
        HEAPF32[i6 >> 2] = +HEAPF32[i10 >> 2];
        HEAPF32[i7 >> 2] = +HEAPF32[i11 >> 2];
        HEAPF32[i8 >> 2] = 1.0;
      } else if ((i16 | 0) == 2) {
        d17 = +HEAPF32[i12 >> 2];
        d18 = +HEAPF32[i11 >> 2];
        HEAPF32[i5 >> 2] = +HEAPF32[i13 >> 2];
        HEAPF32[i6 >> 2] = d17;
        HEAPF32[i7 >> 2] = d18;
        HEAPF32[i8 >> 2] = 1.0;
      } else if ((i16 | 0) == 7) {
        d18 = +HEAPF32[i10 >> 2];
        d17 = +HEAPF32[i14 >> 2];
        HEAPF32[i5 >> 2] = +HEAPF32[i13 >> 2];
        HEAPF32[i6 >> 2] = d18;
        HEAPF32[i7 >> 2] = d17;
        HEAPF32[i8 >> 2] = 1.0;
      }
      i19 = __ZNK4Math7frustum8clipmaskERKNS_5pointE(i1, i4) | 0;
      i20 = i19 & i2;
      i21 = i19 | i15;
      i19 = i16 + 1 | 0;
      if ((i19 | 0) == 8) {
        break;
      } else {
        i2 = i20;
        i15 = i21;
        i16 = i19;
      }
    }
    if ((i21 | 0) == 0) {
      i22 = 0;
      STACKTOP = i3;
      return i22 | 0;
    }
    i22 = (i20 | 0) == 0 ? 2 : 1;
    STACKTOP = i3;
    return i22 | 0;
  }

  // -----------------------------------------------------------------

  var STACKTOP = 0;
  var HEAPF32 = new Float32Array(1000);

  for (var i = 0; i < HEAPF32.length; i++) {
    HEAPF32[i] = 1.0;
  }

  __ZNK4Math7frustum10clipstatusERKNS_4bboxE(0, 0);
  __ZNK4Math7frustum10clipstatusERKNS_4bboxE(0, 0);
  __ZNK4Math7frustum10clipstatusERKNS_4bboxE(0, 0);
  %OptimizeFunctionOnNextCall(__ZNK4Math7frustum10clipstatusERKNS_4bboxE);
  __ZNK4Math7frustum10clipstatusERKNS_4bboxE(0, 0);
})();
