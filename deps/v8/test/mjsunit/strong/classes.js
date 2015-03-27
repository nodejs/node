// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode

'use strong';

class C {}

(function ImmutableClassBindings() {
  class D {}
  assertThrows(function(){ eval("C = 0") }, TypeError);
  assertThrows(function(){ eval("D = 0") }, TypeError);
  assertEquals('function', typeof C);
  assertEquals('function', typeof D);
})();
