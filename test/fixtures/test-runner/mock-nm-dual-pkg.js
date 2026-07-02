'use strict';
const assert = require('node:assert');
const { test } = require('node:test');
const fixture = 'dual-pkg-with-exports';

test('mock node_modules dual package with conditional exports', async (t) => {
  const mock = t.mock.module(fixture, {
    namedExports: { add(x, y) { return 1 + x + y; }, flavor: 'mocked' },
  });

  // CJS require should pick up the mock even though the package's "exports"
  // field maps the "require" condition to a different file than "import".
  const cjsImpl = require(fixture);
  assert.strictEqual(cjsImpl.add(4, 5), 10);
  assert.strictEqual(cjsImpl.flavor, 'mocked');

  // ESM dynamic import should also pick up the mock.
  const esmImpl = await import(fixture);
  assert.strictEqual(esmImpl.add(4, 5), 10);
  assert.strictEqual(esmImpl.flavor, 'mocked');

  mock.restore();

  // After restore, both module systems should see the original exports.
  const restoredCjs = require(fixture);
  assert.strictEqual(restoredCjs.add(4, 5), 9);
  assert.strictEqual(restoredCjs.flavor, 'cjs');

  const restoredEsm = await import(fixture);
  assert.strictEqual(restoredEsm.add(4, 5), 9);
  assert.strictEqual(restoredEsm.flavor, 'esm');
});
