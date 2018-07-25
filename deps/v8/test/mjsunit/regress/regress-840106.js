// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var buffer = new ArrayBuffer(1024 * 1024);
buffer.constructor = {
  [Symbol.species]: new Proxy(function() {}, {
    get: _ => {
      %ArrayBufferNeuter(buffer);
    }
  })
};
var array1 = new Uint8Array(buffer, 0, 1024);
assertThrows(() => new Uint8Array(array1));
assertThrows(() => new Int8Array(array1));
