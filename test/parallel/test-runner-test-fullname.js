'use strict';
const common = require('../common');
const assert = require('node:assert');
const { before, suite, test } = require('node:test');

before(common.mustCall((t) => {
  assert.strictEqual(t.fullName, '<root>');
}));

suite('suite', common.mustCall((t) => {
  assert.strictEqual(t.fullName, 'suite');
  // Test new SuiteContext properties
  assert.strictEqual(typeof t.passed, 'boolean');
  assert.strictEqual(t.attempt, 0);
  // diagnostic() can be called to add diagnostic output
  t.diagnostic('Suite diagnostic message');

  test('test', (t) => {
    assert.strictEqual(t.fullName, 'suite > test');

    t.test('subtest', (t) => {
      assert.strictEqual(t.fullName, 'suite > test > subtest');

      t.test('subsubtest', (t) => {
        assert.strictEqual(t.fullName, 'suite > test > subtest > subsubtest');
      });
    });
  });
}));

test((t) => {
  assert.strictEqual(t.fullName, '<anonymous>');
});

// Test SuiteContext passed, attempt, and diagnostic properties
suite('suite with context checks', common.mustCall((ctx) => {
  assert.strictEqual(ctx.fullName, 'suite with context checks');
  assert.strictEqual(typeof ctx.passed, 'boolean');
  assert.strictEqual(ctx.attempt, 0);
  // Verify diagnostic method is callable
  ctx.diagnostic('Test diagnostic message in suite');

  test('child test', () => {
    // Verify properties are accessible in nested test
    assert.strictEqual(typeof ctx.passed, 'boolean');
    assert.strictEqual(ctx.attempt, 0);
  });
}));
