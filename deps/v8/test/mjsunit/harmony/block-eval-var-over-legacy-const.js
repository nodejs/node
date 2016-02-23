// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-sloppy --harmony-sloppy-let --harmony-sloppy-function

// Var-let conflict in a function throws, even if the var is in an eval

let caught = false;

// Throws at the top level of a function
try {
  (function() {
    let x = 1;
    eval('const x = 2');
  })()
} catch (e) {
  caught = true;
}
assertTrue(caught);

// If the eval is in its own block scope, throws
caught = false;
try {
  (function() {
    let y = 1;
    { eval('const y = 2'); }
  })()
} catch (e) {
  caught = true;
}
assertTrue(caught);

// If the let is in its own block scope, with the eval, throws
caught = false
try {
  (function() {
    {
      let x = 1;
      eval('const x = 2');
    }
  })();
} catch (e) {
  caught = true;
}
assertTrue(caught);

// Legal if the let is no longer visible
caught = false
try {
  (function() {
    {
      let x = 1;
    }
    eval('const x = 2');
  })();
} catch (e) {
  caught = true;
}
assertFalse(caught);

// In global scope
caught = false;
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

// var across with doesn't conflict
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

// var can still conflict with let across a with
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
