'use strict';

const common = require('../common');
const assert = require('assert');

let once = 0;

process.on('beforeExit', () => {
  if (once > 1)
    throw new RangeError('beforeExit should only have been called once!');

  setTimeout(common.noop, 1).unref();
  once++;
});

process.on('exit', (code) => {
  if (code !== 0) return;

  assert.strictEqual(once, 1);
});
