'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const store = new AsyncLocalStorage({ trackAsyncAwait: true });
let checked = 0;

function thenable(expected, count) {
  return {
    then: common.mustCall((cb) => {
      assert.strictEqual(expected, store.getStore());
      checked++;
      cb();
    }, count)
  };
}

function main(n) {
  const firstData = Symbol('first-data');
  const secondData = Symbol('second-data');

  const first = thenable(firstData, 1);
  const second = thenable(secondData, 1);
  const third = thenable(firstData, 2);

  return store.run(firstData, common.mustCall(async () => {
    assert.strictEqual(firstData, store.getStore());
    await first;

    await store.run(secondData, common.mustCall(async () => {
      assert.strictEqual(secondData, store.getStore());
      await second;
      assert.strictEqual(secondData, store.getStore());
    }));

    await Promise.all([ third, third ]);
    assert.strictEqual(firstData, store.getStore());
  }));
}

const outerData = Symbol('outer-data');

Promise.all([
  store.run(outerData, () => Promise.resolve(thenable(outerData))),
  Promise.resolve(3).then(common.mustCall(main)),
  main(1),
  main(2)
]).then(common.mustCall(() => {
  assert.strictEqual(checked, 13);
}));
