// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Slice and splice both try to set the length property of their return
// value. Add a bogus setter to allow that.
Object.defineProperty(Int32Array.prototype, 'length', { set(v) { } });

(function testSlice() {
  var a = new Array();
  a.constructor = Int32Array;
  a.length = 1000; // Make the length >= 1000 so UseSparseVariant returns true.
  assertTrue(a.slice() instanceof Int32Array);
})();

(function testSplice() {
  var a = new Array();
  a.constructor = Int32Array;
  a.length = 1000; // Make the length >= 1000 so UseSparseVariant returns true.
  assertTrue(a.splice(1) instanceof Int32Array);
})();
