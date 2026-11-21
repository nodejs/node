'use strict';
const common = require('../common');
const assert = require('assert');

// setTimeout with a delay of 0 should schedule the callback as soon as
// possible, so that it runs before a timer scheduled with a 1 ms delay.
// See https://github.com/nodejs/node/issues/46596

const order = [];

setTimeout(common.mustCall(() => order.push('one')), 1);
setTimeout(common.mustCall(() => order.push('zero')), 0);

setTimeout(common.mustCall(() => {
  assert.deepStrictEqual(order, ['zero', 'one']);
}), 2);

// A zero-millisecond delay must still be allowed for the promisified variant.
let resolved;
const p = require('node:timers/promises').setTimeout(0);
p.then(common.mustCall(() => { resolved = true; }));

setTimeout(common.mustCall(() => {
  assert.strictEqual(resolved, true);
}), 2);
