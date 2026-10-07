// Flags: --experimental-stream-iter --expose-gc --no-warnings
'use strict';

// Reading the source of a share() must not leave objects behind for every
// read: what a share keeps alive must not grow with the number of batches
// read from its source.

const common = require('../common');
const assert = require('assert');
const { queryObjects } = require('v8');
const { share } = require('stream/iter');

async function test() {
  const reads = 4000;
  const chunk = new Uint8Array(1);
  let i = 0;
  const source = {
    [Symbol.asyncIterator]() { return this; },
    async next() {
      return i++ < reads ?
        { done: false, value: [chunk] } :
        { done: true, value: undefined };
    },
  };
  const shared = share(source, { backpressure: 'unbounded' });
  const a = shared.pull()[Symbol.asyncIterator]();
  const b = shared.pull()[Symbol.asyncIterator]();

  const promises = [];
  for (let n = 0; n < reads; n++) {
    assert.strictEqual((await a.next()).done, false);
    assert.strictEqual((await b.next()).done, false);
    if (n === 1000 || n === 3000) {
      globalThis.gc();
      promises.push(queryObjects(Promise, { format: 'count' }));
    }
  }
  assert.strictEqual((await a.next()).done, true);
  assert.strictEqual((await b.next()).done, true);

  // 2000 reads happened between the two counts. Retaining even one promise
  // per read would add 2000.
  const growth = promises[1] - promises[0];
  assert.ok(growth < 200, `${growth} promises retained across 2000 reads`);
}

test().then(common.mustCall());
