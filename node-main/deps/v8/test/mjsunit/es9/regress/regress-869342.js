// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

function spread(o) { return { ...o }; }

(function setupPolymorphicFeedback() {
  function C1() { this.p0 = 1; }
  function C2() { this.p1 = 2; this.p2 = 3; }
  assertEquals({ p0: 1 }, spread(new C1));
  assertEquals({ p1: 2, p2: 3 }, spread(new C2));
})();

gc(); // Clobber cached map in feedback[0], and check that we don't crash
function C3() { this.p0 = 3; }
assertEquals({ p0: 3 }, spread(new C3));
