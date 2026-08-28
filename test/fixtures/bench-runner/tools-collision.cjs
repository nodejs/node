'use strict';

const { bench, suite } = require('node:bench');

function register(name) {
  bench(name, { params: { size: 1 } }, (b) => {
    b.start();
    process.hrtime.bigint();
    b.end(1);
  });
}

suite('first', () => register('same'));
suite('second', () => register('same'));
