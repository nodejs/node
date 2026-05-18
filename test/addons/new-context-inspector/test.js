'use strict';

const common = require('../../common');
const assert = require('assert');

const binding = require(`./build/${common.buildType}/binding`);

// This verifies that the addon-created context has an independent
// global object.
{
  const ctx = binding.makeContext();
  const result = binding.runInContext(ctx, `
  globalThis.foo = 'bar';
  foo;
  `);
  assert.strictEqual(result, 'bar');
  assert.strictEqual(globalThis.foo, undefined);
}
