// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

delete Float64Array.prototype.__proto__[Symbol.iterator];
let ar = new Float64Array();
Object.defineProperty(ar, "length", {
  get: function () { return 121567939849373; }
});

try { Float64Array.from(ar); } catch (e) {}
