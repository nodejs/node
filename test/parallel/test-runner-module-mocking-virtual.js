// Flags: --experimental-test-module-mocks
'use strict';

// Tests for mocking modules that do not exist with `virtual: true`.
// Refs: https://github.com/nodejs/node/issues/55891

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('registering customization hooks in Workers does not work');
}

const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const path = require('node:path');
const { test } = require('node:test');
const { pathToFileURL } = require('node:url');

test('input validation', (t) => {
  assert.throws(() => {
    t.mock.module('nonexistent-virtual-pkg', { virtual: 'yes' });
  }, { code: 'ERR_INVALID_ARG_TYPE' });
});

test('mocking a module that does not exist throws without virtual', (t) => {
  assert.throws(() => {
    t.mock.module('nonexistent-virtual-pkg', { exports: { foo: 1 } });
  }, { code: 'ERR_MODULE_NOT_FOUND' });
});

test('virtual builtin modules are not supported', (t) => {
  assert.throws(() => {
    t.mock.module('node:nonexistent-virtual-builtin', {
      virtual: true,
      exports: { foo: 1 },
    });
  }, { code: 'ERR_MODULE_NOT_FOUND' });
});

test('bare specifiers can be mocked in both module systems', async (t) => {
  const mock = t.mock.module('nonexistent-virtual-pkg', {
    virtual: true,
    exports: { default: { val: 'default' }, fn: () => 'named' },
  });

  const esmImpl = await import('nonexistent-virtual-pkg');
  // As for other ES module mocks, named exports are not applied to the
  // default export.
  assert.deepStrictEqual(esmImpl.default, { val: 'default' });
  assert.strictEqual(esmImpl.fn(), 'named');

  const cjsImpl = require('nonexistent-virtual-pkg');
  assert.strictEqual(cjsImpl.val, 'default');
  assert.strictEqual(cjsImpl.fn(), 'named');

  mock.restore();

  await assert.rejects(import('nonexistent-virtual-pkg'), {
    code: 'ERR_MODULE_NOT_FOUND',
  });
  assert.throws(() => require('nonexistent-virtual-pkg'), {
    code: 'MODULE_NOT_FOUND',
  });
});

test('relative paths can be mocked in both module systems', async (t) => {
  const mock = t.mock.module('./nonexistent-virtual-file.js', {
    virtual: true,
    exports: { fn: () => 42 },
  });
  const absolutePath = path.join(__dirname, 'nonexistent-virtual-file.js');

  assert.strictEqual(require('./nonexistent-virtual-file.js').fn(), 42);
  assert.strictEqual(require(absolutePath).fn(), 42);
  assert.strictEqual((await import('./nonexistent-virtual-file.js')).fn(), 42);
  assert.strictEqual((await import(pathToFileURL(absolutePath))).fn(), 42);

  mock.restore();

  assert.throws(() => require('./nonexistent-virtual-file.js'), {
    code: 'MODULE_NOT_FOUND',
  });
  await assert.rejects(import('./nonexistent-virtual-file.js'), {
    code: 'ERR_MODULE_NOT_FOUND',
  });
});

test('existing modules are mocked like non-virtual mocks', async (t) => {
  const fixture = fixtures.path('module-mocking', 'basic-cjs.js');
  const original = require(fixture);
  assert.strictEqual(original.string, 'original cjs string');

  // The mock must be visible no matter which specifier resolves to the module.
  t.mock.module(pathToFileURL(fixture), {
    virtual: true,
    exports: { string: 'mocked' },
  });

  assert.strictEqual(require(fixture).string, 'mocked');
  const relativePath = `./${path.relative(__dirname, fixture)}`;
  assert.strictEqual(require(relativePath).string, 'mocked');
  assert.strictEqual((await import(pathToFileURL(fixture))).string, 'mocked');
});

test('cache option works with virtual mocks', async (t) => {
  const mock = t.mock.module('nonexistent-virtual-cached', {
    virtual: true,
    cache: true,
    exports: { default: { n: 1 } },
  });

  const first = require('nonexistent-virtual-cached');
  assert.strictEqual(first.n, 1);
  assert.strictEqual(require('nonexistent-virtual-cached'), first);
  const esm1 = await import('nonexistent-virtual-cached');
  const esm2 = await import('nonexistent-virtual-cached');
  assert.strictEqual(esm1, esm2);

  mock.restore();

  // A new mock of the same specifier must not see the previous cached value.
  t.mock.module('nonexistent-virtual-cached', {
    virtual: true,
    cache: true,
    exports: { default: { n: 2 } },
  });
  assert.strictEqual(require('nonexistent-virtual-cached').n, 2);
});
