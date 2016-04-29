// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-sloppy --harmony-sloppy-let --harmony-sloppy-function

// Var-let conflict in a function throws, even if the var is in an eval

// Throws at the top level of a function
assertThrows(function() {
  let x = 1;
  eval('var x');
}, TypeError);

// If the eval is in its own block scope, throws
assertThrows(function() {
  let y = 1;
  { eval('var y'); }
}, TypeError);

// If the let is in its own block scope, with the eval, throws
assertThrows(function() {
  {
    let x = 1;
    eval('var x');
  }
}, TypeError);

// Legal if the let is no longer visible
assertDoesNotThrow(function() {
  {
    let x = 1;
  }
  eval('var x');
});

// All the same works for const:
// Throws at the top level of a function
assertThrows(function() {
  const x = 1;
  eval('var x');
}, TypeError);

// If the eval is in its own block scope, throws
assertThrows(function() {
  const y = 1;
  { eval('var y'); }
}, TypeError);

// If the const is in its own block scope, with the eval, throws
assertThrows(function() {
  {
    const x = 1;
    eval('var x');
  }
}, TypeError);

// Legal if the const is no longer visible
assertDoesNotThrow(function() {
  {
    const x = 1;
  }
  eval('var x');
});

// In global scope
let caught = false;
try {
  let z = 1;
  eval('var z');
} catch (e) {
  caught = true;
}
assertTrue(caught);

// Let declarations beyond a function boundary don't conflict
caught = false;
try {
  let a = 1;
  (function() {
    eval('var a');
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
      eval("var x");
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
      eval("var x");
    }
  })();
} catch (e) {
  caught = true;
}
assertTrue(caught);

// Functions declared in eval also conflict
caught = false
try {
  (function() {
    {
      let x = 1;
      eval('function x() {}');
    }
  })();
} catch (e) {
  caught = true;
}
assertTrue(caught);

// TODO(littledan): Hoisting x out of the block should be
// prevented in this case BUG(v8:4479)
caught = false
try {
  (function() {
    {
      let x = 1;
      eval('{ function x() {} }');
    }
  })();
} catch (e) {
  caught = true;
}
// TODO(littledan): switch to assertTrue when bug is fixed
assertTrue(caught);
