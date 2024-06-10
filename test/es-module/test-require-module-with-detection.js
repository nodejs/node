// Flags: --experimental-require-module --experimental-detect-module
'use strict';

require('../common');
const assert = require('assert');
const { isModuleNamespaceObject } = require('util/types');

{
  const mod = require('../fixtures/es-modules/loose.js');
  assert.deepStrictEqual({ ...mod }, { default: 'module' });
  assert(isModuleNamespaceObject(mod));
}

{
  const mod = require('../fixtures/es-modules/package-without-type/noext-esm');
  assert.deepStrictEqual({ ...mod }, { default: 'module' });
  assert(isModuleNamespaceObject(mod));
}
