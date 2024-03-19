// Flags: --experimental-require-module
'use strict';

// Tests that require()ing modules without explicit module type information
// warns and errors.
require('../common');
const assert = require('assert');
const { isModuleNamespaceObject } = require('util/types');

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

{
  // .mjs should not be matched as default extensions.
  const id = '../fixtures/es-modules/should-not-be-resolved';
  assert.throws(() => {
    require(id);
  }, {
    code: 'MODULE_NOT_FOUND'
  });
  const mod = require(`${id}.mjs`);
  assert.deepStrictEqual({ ...mod }, { hello: 'world' });
  assert(isModuleNamespaceObject(mod));
}
