'use strict';
// Flags: --expose-gc
// Addons: 7_factory_wrap, 7_factory_wrap_vtable

const { addonPath } = require('../../common/addon-test');
const assert = require('assert');
const test = require(addonPath);
const { gcUntil } = require('../../common/gc');

assert.strictEqual(test.finalizeCount, 0);
async function runGCTests() {
  (() => {
    const obj = test.createObject(10);
    assert.strictEqual(obj.plusOne(), 11);
    assert.strictEqual(obj.plusOne(), 12);
    assert.strictEqual(obj.plusOne(), 13);
  })();
  await gcUntil('test 1', () => (test.finalizeCount === 1));

  (() => {
    const obj2 = test.createObject(20);
    assert.strictEqual(obj2.plusOne(), 21);
    assert.strictEqual(obj2.plusOne(), 22);
    assert.strictEqual(obj2.plusOne(), 23);
  })();
  await gcUntil('test 2', () => (test.finalizeCount === 2));
}
runGCTests();
