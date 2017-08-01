// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.set-typedarray-offset
description: >
  Uses typedArray's internal [[ByteOffset]]
info: >
  22.2.3.23.2 %TypedArray%.prototype.set(typedArray [ , offset ] )
  1. Assert: typedArray has a [[TypedArrayName]] internal slot. If it does not,
  the definition in 22.2.3.23.1 applies.
  ...
  21. Let srcByteOffset be typedArray.[[ByteOffset]].
  ...
includes: [testTypedArray.js]
---*/

var getCalls = 0;
var desc = {
  get: function getLen() {
    getCalls++;
    return 0;
  }
};

Object.defineProperty(TypedArray.prototype, "byteOffset", desc);

testWithTypedArrayConstructors(function(TA) {
  var sample = new TA(2);
  var src = new TA([42, 43]);
  var differentTA = TA === Uint8Array ? Int8Array : Uint8Array;
  var src2 = new differentTA([42, 43]);
  var src3 = new differentTA(sample.buffer, 0, 2);

  Object.defineProperty(TA.prototype, "byteOffset", desc);
  Object.defineProperty(src, "byteOffset", desc);
  Object.defineProperty(src2, "byteOffset", desc);
  Object.defineProperty(src3, "byteOffset", desc);

  sample.set(src);
  sample.set(src2);
  sample.set(src3);

  assert.sameValue(getCalls, 0, "ignores byteOffset properties");
});
