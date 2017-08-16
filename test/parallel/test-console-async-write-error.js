'use strict';
const common = require('../common');
const { Console } = require('console');
const { Writable } = require('stream');
const assert = require('assert');

for (const method of ['dir', 'log', 'warn']) {
  const out = new Writable({
    write: common.mustCall((chunk, enc, callback) => {
      process.nextTick(callback, new Error('foobar'));
    })
  });

  const c = new Console(out, out, true);

  assert.doesNotThrow(() => {
    c[method]('abc');
  });
}
