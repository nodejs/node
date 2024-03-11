// Flags: --experimental-require-module
'use strict';

require('../common');
const assert = require('assert');
const { isModuleNamespaceObject } = require('util/types');

const mod = require('../fixtures/es-modules/package-default-extension/index.mjs');
assert.deepStrictEqual({ ...mod }, { entry: 'mjs' });
assert(isModuleNamespaceObject(mod));

assert.throws(() => {
  const mod = require('../fixtures/es-modules/package-default-extension');
  console.log(mod);  // In case it succeeds, log the result for debugging.
}, {
  code: 'MODULE_NOT_FOUND',
});
