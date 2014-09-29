// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Return the raw CallSites array.
Error.prepareStackTrace = function (a,b) { return b; };

var threw = false;
try {
  new Function({toString:0,valueOf:0});
} catch (e) {
  threw = true;
  // Ensure that the receiver during "new Function" is the global proxy.
  assertEquals(this, e.stack[0].getThis());
}

assertTrue(threw);
