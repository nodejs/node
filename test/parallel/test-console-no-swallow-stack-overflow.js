'use strict';
const common = require('../common');
const assert = require('assert');
const { Console } = require('console');
const { Writable } = require('stream');

for (const method of ['dir', 'log', 'warn']) {
  assert.throws(() => {
    const out = new Writable({
      write: common.mustCall(function write(...args) {
        // Exceeds call stack.
        return write(...args);
      }),
    });
    const c = new Console(out, out, true);

    c[method]('Hello, world!');
  }, { name: 'RangeError' });
}
