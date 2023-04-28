// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function asm(stdlib, foreign, heap) {
  "use asm";
  var heap32 = new stdlib.Uint32Array(heap);
  function f() { return 0; }
  return {f : f};
}

var heap = Reflect.construct(
    SharedArrayBuffer,
    [1024 * 1024],
    ArrayBuffer.prototype.constructor);

asm(this, {}, heap);
