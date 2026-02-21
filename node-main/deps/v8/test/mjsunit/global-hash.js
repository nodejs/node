// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var global = this;

assertEquals("object", typeof global);  // Global object.

var s = new Set();
s.add(global); // Puts a hash code on the global object.
assertTrue(s.has(global));
for (var i = 0; i < 100; i++) {
  // Force rehash. Global object is placed according to the hash code that it
  // gets in the C++ runtime.
  s.add(i);
}

// Hopefully still findable using the JS hash code.
assertTrue(s.has(global));
