// Flags: --experimental-require-module
'use strict';

require('../common');
const assert = require('assert');
const { isModuleNamespaceObject } = require('util/types');

const mod = require('../fixtures/es-modules/package-default-extension/index.mjs');
const namespace = Object.getPrototypeOf(mod);
assert.deepStrictEqual({ ...namespace }, { entry: 'mjs' });
assert(isModuleNamespaceObject(namespace));

assert.throws(() => {
  const mod = require('../fixtures/es-modules/package-default-extension');
  console.log(mod);  // In case it succeeds, log the result for debugging.
}, {
  code: 'MODULE_NOT_FOUND',
});
