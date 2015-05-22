spdx.js
=======

[![npm version](https://img.shields.io/npm/v/spdx.svg)](https://www.npmjs.com/package/spdx)
[![SPDX License Expression Syntax version](https://img.shields.io/badge/SPDX-2.0-blue.svg)](http://spdx.org/SPDX-specifications/spdx-version-2.0)
[![license](https://img.shields.io/badge/license-Apache--2.0-303284.svg)](http://www.apache.org/licenses/LICENSE-2.0)
[![build status](https://img.shields.io/travis/kemitchell/spdx.js.svg)](http://travis-ci.org/kemitchell/spdx.js)

SPDX License Expression Syntax parser

<!--js
  // The fenced code blocks below are run as tests with `jsmd`.
  // The following `require` call brings the module.
  // Use `require ('spdx')` in your own code.
  var spdx = require('./');
  var package = require('./package.json');
-->

Simple License Expressions
--------------------------
```js
spdx.valid('Invalid-Identifier'); // => null
spdx.valid('GPL-2.0'); // => true
spdx.valid('GPL-2.0+'); // => true
spdx.valid('LicenseRef-23'); // => true
spdx.valid('LicenseRef-MIT-Style-1'); // => true
spdx.valid('DocumentRef-spdx-tool-1.2:LicenseRef-MIT-Style-2'); // => true
```

Composite License Expressions
-----------------------------

### Disjunctive `OR` Operator
```js
spdx.valid('(LGPL-2.1 OR MIT)'); // => true
spdx.valid('(LGPL-2.1 OR MIT OR BSD-3-Clause)'); // => true
```

### Conjunctive `AND` Operator
```js
spdx.valid('(LGPL-2.1 AND MIT)'); // => true
spdx.valid('(LGPL-2.1 AND MIT AND BSD-2-Clause)'); // => true
```

### Exception `WITH` Operator
```js
spdx.valid('(GPL-2.0+ WITH Bison-exception-2.2)'); // => true
```

### Order of Precedence and Parentheses
```js
var firstAST = {
  left: {license: 'LGPL-2.1'},
  conjunction: 'or',
  right: {
    left: {license: 'BSD-3-Clause'},
    conjunction: 'and',
    right: {license: 'MIT'}
  }
};
spdx.parse('(LGPL-2.1 OR BSD-3-Clause AND MIT)'); // => firstAST

var secondAST = {
  left: {license: 'MIT'},
  conjunction: 'and',
  right: {
    left: {license: 'LGPL-2.1', plus: true},
    conjunction: 'and',
    right: {license: 'BSD-3-Clause'}
  }
};
spdx.parse('(MIT AND (LGPL-2.1+ AND BSD-3-Clause))'); // => secondAST
```

Strict Whitespace Rules
-----------------------
```js
spdx.valid('MIT '); // => false
spdx.valid(' MIT'); // => false
spdx.valid('MIT  AND  BSD-3-Clause'); // => false
```

Identifier Lists
----------------
```js
Array.isArray(spdx.licenses); // => true
spdx.licenses.indexOf('ISC') > -1; // => true
spdx.licenses.indexOf('Apache-1.7') > -1; // => false
spdx.licenses.every(function(element) {
  return typeof element === 'string';
}); // => true

Array.isArray(spdx.exceptions); // => true
spdx.exceptions.indexOf('GCC-exception-3.1') > -1; // => true
spdx.exceptions.every(function(element) {
  return typeof element === 'string';
}); // => true
```

Comparison
----------
```js
spdx.gt('GPL-3.0', 'GPL-2.0'); // => true
spdx.lt('MPL-1.0', 'MPL-2.0'); // => true

spdx.gt('LPPL-1.3a', 'LPPL-1.0'); // => true
spdx.gt('LPPL-1.3a', 'LPPL-1.3a'); // => false
spdx.gt('MIT', 'ISC'); // => false

try {
  spdx.gt('(MIT OR ISC)', 'GPL-3.0');
} catch (error) {
  error.message; // => '"(MIT OR ISC)" is not a simple license identifier'
}

spdx.satisfies('MIT', 'MIT'); // => true
spdx.satisfies('MIT', '(ISC OR MIT)'); // => true
spdx.satisfies('Zlib', '(ISC OR (MIT OR Zlib))'); // => true
spdx.satisfies('GPL-3.0', '(ISC OR MIT)'); // => false
spdx.satisfies('GPL-2.0', 'GPL-2.0+'); // => true
spdx.satisfies('GPL-3.0', 'GPL-2.0+'); // => true
spdx.satisfies('GPL-1.0', 'GPL-2.0+'); // => false

spdx.satisfies('GPL-2.0', 'GPL-2.0+ WITH Bison-exception-2.2'); // => false
spdx.satisfies('GPL-3.0 WITH Bison-exception-2.2', 'GPL-2.0+ WITH Bison-exception-2.2'); // => true

spdx.satisfies('(MIT OR GPL-2.0)', '(ISC OR MIT)'); // => true
spdx.satisfies('(MIT AND GPL-2.0)', '(MIT OR GPL-2.0)'); // => true
spdx.satisfies('(MIT AND GPL-2.0)', '(ISC OR GPL-2.0)'); // => false
```

Version Metadata
----------------
```js
spdx.specificationVersion; // => '2.0'
spdx.implementationVersion; // => package.version
```

The Specification
-----------------
[The Software Package Data Exchange (SPDX) specification](http://spdx.org) is the work of the [Linux Foundation](http://www.linuxfoundation.org) and its contributors, and is licensed under the terms of [the Creative Commons Attribution License 3.0 Unported (SPDX: "CC-BY-3.0")](http://spdx.org/licenses/CC-BY-3.0). "SPDX" is a United States federally registered trademark of the Linux Foundation.
