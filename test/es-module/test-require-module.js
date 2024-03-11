// Flags: --experimental-require-module
'use strict';

const common = require('../common');
const assert = require('assert');
const { isModuleNamespaceObject } = require('util/types');

common.expectWarning(
  'ExperimentalWarning',
  'Support for loading ES Module in require() is an experimental feature ' +
  'and might change at any time'
);

// Test named exports.
{
  const mod = require('../fixtures/es-module-loaders/module-named-exports.mjs');
  assert.deepStrictEqual({ ...mod }, { foo: 'foo', bar: 'bar' });
  assert(isModuleNamespaceObject(mod));
}

// Test ESM that import ESM.
{
  const mod = require('../fixtures/es-modules/import-esm.mjs');
  assert.deepStrictEqual({ ...mod }, { hello: 'world' });
  assert(isModuleNamespaceObject(mod));
}

// Test ESM that import CJS.
{
  const mod = require('../fixtures/es-modules/cjs-exports.mjs');
  assert.deepStrictEqual({ ...mod }, {});
  assert(isModuleNamespaceObject(mod));
}

// Test ESM that require() CJS.
{
  const mjs = require('../common/index.mjs');
  // Only comparing a few properties because the ESM version of test/common doesn't
  // re-export everything from the CJS version.
  assert.strictEqual(common.mustCall, mjs.mustCall);
  assert.strictEqual(common.localIPv6Hosts, mjs.localIPv6Hosts);
  assert(!isModuleNamespaceObject(common));
  assert(isModuleNamespaceObject(mjs));
}

// Test "type": "module" and "main" field in package.json.
// Also, test default export.
{
  const mod = require('../fixtures/es-modules/package-type-module');
  assert.deepStrictEqual({ ...mod }, { default: 'package-type-module' });
  assert(isModuleNamespaceObject(mod));
}

// Test data: import.
{
  const mod = require('../fixtures/es-modules/data-import.mjs');
  assert.deepStrictEqual({ ...mod }, {
    data: { hello: 'world' },
    id: 'data:text/javascript,export default %7B%20hello%3A%20%22world%22%20%7D'
  });
  assert(isModuleNamespaceObject(mod));
}
