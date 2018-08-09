'use strict';

const common = require('../common');
const assert = require('assert');

const resolveMessage = 'First call';
const swallowedResolve = 'Swallowed resolve';
const rejection = new Error('Swallowed reject');

const expected = [
  'resolveAfterResolved',
  swallowedResolve,
  'Resolve was called more than once.',
  'rejectAfterResolved',
  rejection,
  'Reject was called after resolve.'
];

function handler(type, p, reason, message) {
  assert.strictEqual(type, expected.shift());
  assert.deepStrictEqual(p, Promise.resolve(resolveMessage));
  assert.strictEqual(reason, expected.shift());
  assert.strictEqual(message, expected.shift());
}

process.on('multipleResolves', common.mustCall(handler, 2));

new Promise((resolve, reject) => {
  resolve(resolveMessage);
  resolve(swallowedResolve);
  reject(rejection);
});
