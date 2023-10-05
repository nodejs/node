// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --shared-string-table --harmony-struct

(function TestArrayConcatSharedArrayOnArrayProto() {
  let sarr = new SharedArray();
  Object.defineProperty(
      Array.prototype, 0, {writable: true, enumerable: true, value: 42});
  print(Object.setPrototypeOf(Array.prototype, sarr).concat());
})();
