// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var o = { f: function() { throw new Error(); } };
o.g1 = function() { o.f() }
o.g2 = o.g1;
o.h = function() { o.g1() }

Error.prepareStackTrace = function(e, frames) { return frames; }

try {
  o.h();
} catch (e) {
  var frames = e.stack;
  assertEquals("f", frames[0].getMethodName());
  assertEquals(null, frames[1].getMethodName());
  assertEquals("h", frames[2].getMethodName());
  assertEquals(null, frames[3].getMethodName());
}
