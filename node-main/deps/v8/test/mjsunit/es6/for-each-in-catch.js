// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function checkIsRedeclarationError(code) {
  try {
    eval(`
      checkIsRedeclarationError: {
        break checkIsRedeclarationError;
        ${code}
      }
    `);
    assertUnreachable();
  } catch (e) {
    assertInstanceof(e, SyntaxError);
    assertTrue(e.toString().includes("has already been declared"));
  }
}

function checkIsNotRedeclarationError(code) {
  assertDoesNotThrow(() => eval(`
    checkIsNotRedeclarationError_label: {
      break checkIsNotRedeclarationError_label;
      ${code}
    }
  `));
}


let var_e = [
  'var e',
  'var {e}',
  'var {f, e}',
  'var [e]',
  'var {f:e}',
  'var [[[], e]]'
];

let not_var_e = [
  'var f',
  'var {}',
  'var {e:f}',
  'e',
  '{e}',
  'let e',
  'const e',
  'let {e}',
  'const {e}',
  'let [e]',
  'const [e]',
  'let {f:e}',
  'const {f:e}'
];

// Check that both `for (var ... of ...)` and `for (var ... in ...)`
// can redeclare a simple catch variable.
for (let binding of var_e) {
  checkIsNotRedeclarationError(`
    try {
      throw 0;
    } catch (e) {
      for (${binding} of []);
    }
  `);

  checkIsNotRedeclarationError(`
    try {
      throw 0;
    } catch (e) {
      for (${binding} in []);
    }
  `);
}

// Check that the above applies even for nested catches.
for (let binding of var_e) {
  checkIsNotRedeclarationError(`
    try {
      throw 0;
    } catch (e) {
      try {
        throw 1;
      } catch (f) {
        try {
          throw 2;
        } catch ({}) {
          for (${binding} of []);
        }
      }
    }
  `);

  checkIsNotRedeclarationError(`
    try {
      throw 0;
    } catch (e) {
      try {
        throw 1;
      } catch (f) {
        try {
          throw 2;
        } catch ({}) {
          for (${binding} in []);
        }
      }
    }
  `);
}

// Check that the above applies if a declaration scope is between the
// catch and the loop.
for (let binding of var_e) {
  checkIsNotRedeclarationError(`
    try {
      throw 0;
    } catch (e) {
      (()=>{for (${binding} of []);})();
    }
  `);

  checkIsNotRedeclarationError(`
    try {
      throw 0;
    } catch (e) {
      (function() {
        for (${binding} of []);
      })();
    }
  `);
}

// Check that there is no error when not declaring a var named e.
for (let binding of not_var_e) {
  checkIsNotRedeclarationError(`
    try {
      throw 0;
    } catch (e) {
      for (${binding} of []);
    }
  `);
}

// Check that there is an error for both for-in and for-of when redeclaring
// a non-simple catch parameter.
for (let binding of var_e) {
  checkIsRedeclarationError(`
    try {
      throw 0;
    } catch ({e}) {
      for (${binding} of []);
    }
  `);

  checkIsRedeclarationError(`
    try {
      throw 0;
    } catch ({e}) {
      for (${binding} in []);
    }
  `);
}

// Check that the above error occurs even for nested catches.
for (let binding of var_e) {
  checkIsRedeclarationError(`
    try {
      throw 0;
    } catch ({e}) {
      try {
        throw 1;
      } catch (f) {
        try {
          throw 2;
        } catch ({}) {
          for (${binding} of []);
        }
      }
    }
  `);

  checkIsRedeclarationError(`
    try {
      throw 0;
    } catch ({e}) {
      try {
        throw 1;
      } catch (f) {
        try {
          throw 2;
        } catch ({}) {
          for (${binding} in []);
        }
      }
    }
  `);
}
