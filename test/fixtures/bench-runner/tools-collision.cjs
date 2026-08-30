'use strict';

const { bench, suite } = require('node:bench');

function register(name) {
  bench(name, { params: { size: 1 } }, (b) => {
    b.record({ duration_ns: 1n, operations: 1 });
  });
}

suite('first', () => register('same'));
suite('second', () => register('same'));
