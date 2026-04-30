'use strict';
require('../common');
const assert = require('node:assert');
const { test, getTestContext, describe, it } = require('node:test');

// Outside a test — must return undefined
assert.strictEqual(getTestContext(), undefined);

test('getTestContext returns current context inside test', async () => {
  const ctx = getTestContext();
  assert.ok(ctx !== undefined);
  assert.strictEqual(ctx.name, 'getTestContext returns current context inside test');
  assert.strictEqual(typeof ctx.signal, 'object');
  assert.strictEqual(typeof ctx.fullName, 'string');
});

test('getTestContext works in nested test', async (t) => {
  await t.test('child', async () => {
    const ctx = getTestContext();
    assert.ok(ctx !== undefined);
    assert.strictEqual(ctx.name, 'child');
  });
});

describe('getTestContext works in describe/it', () => {
  it('has correct name', () => {
    const ctx = getTestContext();
    assert.ok(ctx !== undefined);
    assert.strictEqual(ctx.name, 'has correct name');
  });
});

describe('getTestContext returns SuiteContext in suite', () => {
  it('suite context is available', () => {
    const ctx = getTestContext();
    assert.ok(ctx !== undefined);
    // Suite name appears as parent in nested test context
    assert.strictEqual(typeof ctx.signal, 'object');
    assert.strictEqual(typeof ctx.fullName, 'string');
  });
});

test('getTestContext works in test body during async operations', async (t) => {
  const ctx = getTestContext();
  assert.ok(ctx !== undefined);
  assert.strictEqual(ctx.name, 'getTestContext works in test body during async operations');

  // Also works in nested async context
  const ctxInSetImmediate = await new Promise((resolve) => {
    setImmediate(() => resolve(getTestContext()));
  });
  assert.ok(ctxInSetImmediate !== undefined);
  assert.strictEqual(ctxInSetImmediate.name, 'getTestContext works in test body during async operations');
});
