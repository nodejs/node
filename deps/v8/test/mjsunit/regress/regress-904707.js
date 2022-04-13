// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

delete Float64Array.prototype.__proto__[Symbol.iterator];

let a = new Float64Array(9);
Object.defineProperty(a, "length", {
  get: function () { %ArrayBufferDetach(a.buffer); return 6; }
});

Float64Array.from(a);
