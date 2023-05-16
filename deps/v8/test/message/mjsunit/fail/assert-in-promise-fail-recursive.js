// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let count = 2;
TypeError.prototype.__defineGetter__("name", function () {
  if (count <= 0) return;
  count--;
  WebAssembly.compile();
});
;
console.log(new TypeError());
