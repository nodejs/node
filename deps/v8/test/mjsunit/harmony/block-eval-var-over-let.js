// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-sloppy --harmony-sloppy-let --harmony-sloppy-function --no-legacy-const

// Var-let conflict in a function throws, even if the var is in an eval

let caught = false;

// Throws at the top level of a function
try {
  (function() {
    let x = 1;
    eval('var x = 2');
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
    { eval('var y = 2'); }
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
      eval('var x = 2');
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
    eval('var x = 2');
  })();
} catch (e) {
  caught = true;
}
assertFalse(caught);

// All the same works for const:
// Throws at the top level of a function
try {
  (function() {
    const x = 1;
    eval('var x = 2');
  })();
} catch (e) {
  caught = true;
}
assertTrue(caught);

// If the eval is in its own block scope, throws
caught = false;
try {
  (function() {
    const y = 1;
    { eval('var y = 2'); }
  })();
} catch (e) {
  caught = true;
}
assertTrue(caught);

// If the const is in its own block scope, with the eval, throws
caught = false
try {
  (function() {
    {
      const x = 1;
      eval('var x = 2');
    }
  })();
} catch (e) {
  caught = true;
}
assertTrue(caught);

// Legal if the const is no longer visible
caught = false
try {
  (function() {
    {
      const x = 1;
    }
    eval('var x = 2');
  })();
} catch (e) {
  caught = true;
}
assertFalse(caught);

// In global scope
caught = false;
try {
  let z = 1;
  eval('var z = 2');
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
      eval("var x = 2;");
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
      eval("var x = 2;");
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
