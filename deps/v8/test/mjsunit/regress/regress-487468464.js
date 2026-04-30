// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --track-array-buffer-views

Object.defineProperty(Uint8Array.prototype.__proto__, "length", {
  get() {
    return 42;
  }
});
let ta = new Uint8Array();
let ta_iter = ta[Symbol.iterator]();
%ArrayBufferDetach(ta.buffer);
assertThrows(() => ta_iter.next().value, TypeError);
