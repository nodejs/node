'use strict';
const common = require('../common');
const assert = require('node:assert');
const { before, suite, test } = require('node:test');

before(common.mustCall((t) => {
  assert.strictEqual(t.fullName, '<root>');
}));

suite('suite', common.mustCall((t) => {
  assert.strictEqual(t.fullName, 'suite');

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
