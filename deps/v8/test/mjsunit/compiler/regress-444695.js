// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var foo = (function(stdlib, foreign, heap) {
  "use asm";
  var MEM = new stdlib.Uint8Array(heap);
  function foo(x) { MEM[x | 0] *= 0; }
  return {foo: foo};
})(this, {}, new ArrayBuffer(1)).foo;
foo(-926416896 * 8 * 1024);
