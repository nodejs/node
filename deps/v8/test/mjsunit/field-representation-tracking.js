// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --modify-field-representation-inplace

// Test that s->t field representation changes are done in-place.
(function() {
  function O(x) { this.x = x; }

  const a = new O(42);
  const b = new O(-8);
  assertTrue(%HaveSameMap(a, b));
  a.x = null;
  assertTrue(%HaveSameMap(a, b));
  b.x = null;
  assertTrue(%HaveSameMap(a, b));
})();

// Test that h->t field representation changes are done in-place.
(function() {
  function O(x) { this.x = x; }

  const a = new O(null);
  const b = new O("Hello");
  assertTrue(%HaveSameMap(a, b));
  a.x = 1;
  assertTrue(%HaveSameMap(a, b));
  b.x = 2;
  assertTrue(%HaveSameMap(a, b));
})();
