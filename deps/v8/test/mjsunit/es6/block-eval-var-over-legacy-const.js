// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-sloppy --harmony-sloppy-let --harmony-sloppy-function
// Flags: --legacy-const

// Legacy-const-let conflict in a function throws, even if the legacy const
// is in an eval

// Throws at the top level of a function
assertThrows(function() {
  let x = 1;
  eval('const x = 2');
}, TypeError);

// If the eval is in its own block scope, throws
assertThrows(function() {
  let y = 1;
  { eval('const y = 2'); }
}, TypeError);

// If the let is in its own block scope, with the eval, throws
assertThrows(function() {
  {
    let x = 1;
    eval('const x = 2');
  }
}, TypeError);

// Legal if the let is no longer visible
assertDoesNotThrow(function() {
  {
    let x = 1;
  }
  eval('const x = 2');
});

// In global scope
let caught = false;
try {
  let z = 1;
  eval('const z = 2');
} catch (e) {
  caught = true;
}
assertTrue(caught);

// Let declarations beyond a function boundary don't conflict
caught = false;
try {
  let a = 1;
  (function() {
    eval('const a');
  })();
} catch (e) {
  caught = true;
}
assertFalse(caught);

// legacy const across with doesn't conflict
caught = false;
try {
  (function() {
    with ({x: 1}) {
      eval("const x = 2;");
    }
  })();
} catch (e) {
  caught = true;
}
assertFalse(caught);

// legacy const can still conflict with let across a with
caught = false;
try {
  (function() {
    let x;
    with ({x: 1}) {
      eval("const x = 2;");
    }
  })();
} catch (e) {
  caught = true;
}
assertTrue(caught);
