// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var thrower = {[Symbol.toPrimitive]: function(e) { throw e }};
try {
  for (var i = 0; i < 10; i++) { }
  for (var i = 0.5; i < 100000; ++i) { }
  for (var i = 16 | 0 || 0 || this || 1; i;) { String.fromCharCode(thrower); }
} catch (e) { }
