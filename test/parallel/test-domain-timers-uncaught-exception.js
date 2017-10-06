'use strict';
const common = require('../common');

// This test ensures that the timer callbacks are called in the order in which
// they were created in the event of an unhandled exception in the domain.

const domain = require('domain').create();
const assert = require('assert');

let first = false;

domain.run(function() {
  setTimeout(() => { throw new Error('FAIL'); }, 1);
  setTimeout(() => { first = true; }, 1);
  setTimeout(() => { assert.strictEqual(first, true); }, 2);

  // Ensure that 2 ms have really passed
  let i = 1e6;
  while (i--);
});

domain.once('error', common.mustCall((err) => {
  assert(err);
  assert.strictEqual(err.message, 'FAIL');
}));
