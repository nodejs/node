'use strict';

const common = require('../common');
const assert = require('assert');

// This test ensures that Node.js will not ignore tty 'readable' subscribers
// when it's the only tty subscriber and the only thing keeping event loop alive
// https://github.com/nodejs/node/issues/20503

process.stdin.setEncoding('utf8');

const expectedInput = ['foo', 'bar', null];

process.stdin.on('readable', common.mustCall(function() {
  const data = process.stdin.read();
  assert.strictEqual(data, expectedInput.shift());
}, 3)); // first 2 data, then end

process.stdin.on('end', common.mustCall());

setTimeout(() => {
  process.stdin.push('foo');
  process.nextTick(() => {
    process.stdin.push('bar');
    process.nextTick(() => {
      process.stdin.push(null);
    });
  });
}, 1);
