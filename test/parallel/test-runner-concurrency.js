'use strict';
const common = require('../common');
const { describe, it, test } = require('node:test');
const assert = require('assert');

describe('Concurrency option (boolean) = true ', { concurrency: true }, () => {
  let isFirstTestOver = false;
  it('should start the first test', () => new Promise((resolve) => {
    setImmediate(() => { isFirstTestOver = true; resolve(); });
  }));
  it('should start before the previous test ends', () => {
    // Should work even on single core CPUs
    assert.strictEqual(isFirstTestOver, false);
  });
});

describe(
  'Concurrency option (boolean) = false ',
  { concurrency: false },
  () => {
    let isFirstTestOver = false;
    it('should start the first test', () => new Promise((resolve) => {
      setImmediate(() => { isFirstTestOver = true; resolve(); });
    }));
    it('should start after the previous test ends', () => {
      assert.strictEqual(isFirstTestOver, true);
    });
  }
);

{
  // Make sure tests run in order when root concurrency is 1 (default)
  const tree = [];
  const expectedTestTree = common.mustCall(() => {
    assert.deepStrictEqual(tree, [
      'suite 1', 'nested', 'suite 2',
      '1', '2', 'nested 1', 'nested 2',
      'test', 'test 1', 'test 2',
    ]);
  });

  describe('suite 1', () => {
    tree.push('suite 1');
    it('1', () => tree.push('1'));
    it('2', () => tree.push('2'));

    describe('nested', () => {
      tree.push('nested');
      it('nested 1', () => tree.push('nested 1'));
      it('nested 2', () => tree.push('nested 2'));
    });
  });

  test('test', async (t) => {
    tree.push('test');
    await t.test('test1', () => tree.push('test 1'));
    await t.test('test 2', () => tree.push('test 2'));
  });

  describe('suite 2', () => {
    tree.push('suite 2');
    it('should run after other suites', expectedTestTree);
  });
}
