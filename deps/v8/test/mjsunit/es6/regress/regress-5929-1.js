// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var buf = new ArrayBuffer(0x10000);
var arr = new Uint8Array(buf).fill(55);
var tmp = {};
tmp[Symbol.toPrimitive] = function () {
  %ArrayBufferNeuter(arr.buffer);
  return 50;
}
arr.copyWithin(tmp);
