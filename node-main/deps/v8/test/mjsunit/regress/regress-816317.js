// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let a = new Float64Array(15);
Object.defineProperty(a, "length", {
  get: function () {
    return 6;
  }
});
delete a.__proto__.__proto__[Symbol.iterator];
Float64Array.from(a);
