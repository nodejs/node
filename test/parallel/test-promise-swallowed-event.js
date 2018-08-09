'use strict';

const common = require('../common');
const assert = require('assert');

const rejection = new Error('Swallowed reject');
const rejection2 = new TypeError('Weird');
const resolveMessage = 'First call';
const rejectPromise = new Promise((r) => setTimeout(r, 10, rejection2));
const swallowedResolve = 'Swallowed resolve';
const swallowedResolve2 = 'Foobar';

process.on('multipleResolves', common.mustCall(handler, 4));

const p1 = new Promise((resolve, reject) => {
  resolve(resolveMessage);
  resolve(swallowedResolve);
  reject(rejection);
});

const p2 = new Promise((resolve, reject) => {
  reject(rejectPromise);
  resolve(swallowedResolve2);
  reject(rejection2);
}).catch(common.mustCall((exception) => {
  assert.strictEqual(exception, rejectPromise);
}));

const expected = [
  'resolve',
  p1,
  swallowedResolve,
  'reject',
  p1,
  rejection,
  'resolve',
  p2,
  swallowedResolve2,
  'reject',
  p2,
  rejection2
];

let count = 0;

function handler(type, promise, reason) {
  assert.strictEqual(type, expected.shift());
  // In the first two cases the promise is identical because it's not delayed.
  // The other two cases are not identical, because the `promise` is caught in a
  // state when it has no knowledge about the `.catch()` handler that is
  // attached to it right afterwards.
  if (count++ < 2) {
    assert.strictEqual(promise, expected.shift());
  } else {
    assert.notStrictEqual(promise, expected.shift());
  }
  assert.strictEqual(reason, expected.shift());
}
