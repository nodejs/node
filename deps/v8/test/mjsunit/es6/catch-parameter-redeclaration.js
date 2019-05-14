// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function checkIsRedeclarationError(code) {
  try {
    eval(`
checkIsRedeclarationError : {
  break checkIsRedeclarationError;
${code}
}
`);
    assertUnreachable();
  } catch(e) {
    assertInstanceof(e, SyntaxError );
    assertTrue( e.toString().indexOf("has already been declared") >= 0 );
  }
}

function checkIsNotRedeclarationError(code) {
  assertDoesNotThrow(()=>eval(`
checkIsNotRedeclarationError_label : {
  break checkIsNotRedeclarationError_label;
${code}
}
`));
}


let lexical_e = [
  'let e',
  'let f, g, e',
  'let [f] = [], [] = [], e = e, h',
  'let {e} = 0',
  'let {f, e} = 0',
  'let {f, g} = 0, {e} = 0',
  'let {f = 0, e = 1} = 0',
  'let [e] = 0',
  'let [f, e] = 0',
  'let {f:e} = 0',
  'let [[[], e]] = 0',
  'const e = 0',
  'const f = 0, g = 0, e = 0',
  'const {e} = 0',
  'const [e] = 0',
  'const {f:e} = 0',
  'const [[[], e]] = 0',
  'function e(){}',
  'function* e(){}',
];

let not_lexical_e = [
  'var e',
  'var f, e',
  'var {e} = 0',
  'let {} = 0',
  'let {e:f} = 0',
  '{ function e(){} }'
];

// Check that lexical declarations cannot override a simple catch parameter
for (let declaration of lexical_e) {
  checkIsRedeclarationError(`
try {
  throw 0;
} catch(e) {
  ${declaration}
}
`);
}

// Check that lexical declarations cannot override a complex catch parameter
for (let declaration of lexical_e) {
  checkIsRedeclarationError(`
try {
  throw 0;
} catch({e}) {
  ${declaration}
}
`);
}

// Check that non-lexical declarations can override a simple catch parameter
for (let declaration of not_lexical_e) {
  checkIsNotRedeclarationError(`
try {
  throw 0;
} catch(e) {
  ${declaration}
}
`);
}

// Check that the above error does not occur if a declaration scope is between
// the catch and the loop.
for (let declaration of lexical_e) {
  checkIsNotRedeclarationError(`
try {
  throw 0;
} catch(e) {
  (()=>{${declaration}})();
}
`);

  checkIsNotRedeclarationError(`
try {
  throw 0;
} catch(e) {
  (function(){${declaration}})();
}
`);
}
