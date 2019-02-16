'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

const w = new Worker(
  `const fn = (err) => {
    if (err.message === 'fhqwhgads')
      throw new Error('come on');
    return 'This is my custom stack trace!';
  };
  Error.prepareStackTrace = fn;
  throw new Error('fhqwhgads');
  `,
  { eval: true }
);
w.on('message', common.mustNotCall());
w.on('error', common.mustCall((err) => {
  assert.strictEqual(err.stack, undefined);
  assert.strictEqual(err.message, 'fhqwhgads');
  assert.strictEqual(err.name, 'Error');
}));
