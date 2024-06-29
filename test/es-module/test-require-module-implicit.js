// Flags: --experimental-require-module
'use strict';

// Tests that require()ing .mjs should not be matched as default extensions.
require('../common');
const assert = require('assert');
const { isModuleNamespaceObject } = require('util/types');

{
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
