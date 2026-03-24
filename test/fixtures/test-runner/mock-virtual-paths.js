'use strict';
const assert = require('node:assert');
const { test } = require('node:test');

// This test verifies that a virtual mock of an existing package intercepts
// ALL resolution paths to that module, not just the exact specifier used
// to create the mock. This is critical to avoid multiple sources of truth.

test('virtual mock intercepts all resolution paths to the same module', async (t) => {
  t.mock.module('reporter-cjs', {
    virtual: true,
    namedExports: { fn() { return 'mocked'; } },
  });

  // Access via package name (the specifier used to create the mock).
  const byName = require('reporter-cjs');
  assert.strictEqual(byName.fn(), 'mocked');

  // Access via relative path to the actual file on disk. This resolves to
  // the same canonical file:// URL, so it must also return the mock.
  const byRelativePath = require('./node_modules/reporter-cjs/index.js');
  assert.strictEqual(byRelativePath.fn(), 'mocked');

  // Access via import() by package name.
  const byImport = await import('reporter-cjs');
  assert.strictEqual(byImport.fn(), 'mocked');
});
