// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --track-array-buffer-views

// Update map with detach transition and properties
(() => {
  let ab = new ArrayBuffer();
  let ta = new Int32Array(ab);
  ta[undefined] = undefined;
  %ArrayBufferDetach(ab);
  let ta2 = new Int32Array();
  ta2[undefined] = undefined;
  Object.defineProperty(ta2, undefined, {value: {}});
})();

// Update detached map
(function () {
  let ab = new ArrayBuffer();
  let ta = new Int32Array(ab);
  %ArrayBufferDetach(ab);
  ta.__proto__ = {};
})();
