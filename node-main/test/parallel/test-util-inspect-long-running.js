'use strict';

require('../common');

// Test that huge objects don't crash due to exceeding the maximum heap size.

const util = require('util');

// Create a difficult to stringify object. Without the artificial limitation
// this would crash or throw an maximum string size error.
let last = {};
const obj = last;

for (let i = 0; i < 1000; i++) {
  last.next = { circular: obj, last, obj: { a: 1, b: 2, c: true } };
  last = last.next;
  obj[i] = last;
}

util.inspect(obj, { depth: Infinity });
