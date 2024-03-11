// Flags: --experimental-require-module
'use strict';

// Tests that require()ing modules without explicit module type information
// warns and errors.
const common = require('../common');
const assert = require('assert');

common.expectWarning(
  'Warning',
  'The module being require()d looks like an ES module, but it is ' +
  'not explicitly marked with "type": "module" in the package.json ' +
  'or with a .mjs extention. To enable automatic detection of module ' +
  'syntax in require(), use --experimental-require-module-with-detection.'
);

common.expectWarning(
  'ExperimentalWarning',
  'Support for loading ES Module in require() is an experimental feature ' +
  'and might change at any time'
);

assert.throws(() => {
  require('../fixtures/es-modules/package-without-type/noext-esm');
}, {
  message: /Unexpected token 'export'/
});

assert.throws(() => {
  require('../fixtures/es-modules/loose.js');
}, {
  message: /Unexpected token 'export'/
});
