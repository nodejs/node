// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-sharedarraybuffer
var sab = new SharedArrayBuffer(8);
var ta = new Int32Array(sab);
ta.__defineSetter__('length', function() {;});
assertThrows(function() {
  Atomics.compareExchange(ta, 4294967295, 0, 0);
}, RangeError);
