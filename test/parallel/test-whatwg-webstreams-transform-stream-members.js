'use strict';

require('../common');
const assert = require('node:assert');

const combinations = [
  ((t) => [t, t.readable])(new TransformStream()),
  ((t) => [t.readable, t])(new TransformStream()),
  ((t) => [t, t.writable])(new TransformStream()),
  ((t) => [t.writable, t])(new TransformStream()),
];

for (const combination of combinations) {
  assert.throws(() => structuredClone(combination, { transfer: combination }), {
    constructor: DOMException,
    name: 'DataCloneError',
    code: 25,
  });
}
