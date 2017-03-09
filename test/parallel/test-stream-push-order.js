'use strict';
require('../common');
const Readable = require('stream').Readable;
const assert = require('assert');

const s = new Readable({
  highWaterMark: 20,
  encoding: 'ascii'
});

const list = ['1', '2', '3', '4', '5', '6'];

s._read = function(n) {
  const one = list.shift();
  if (!one) {
    s.push(null);
  } else {
    const two = list.shift();
    s.push(one);
    s.push(two);
  }
};

s.read(0);

// ACTUALLY [1, 3, 5, 6, 4, 2]

process.on('exit', function() {
  assert.deepStrictEqual(s._readableState.buffer.join(','), '1,2,3,4,5,6');
  console.log('ok');
});
