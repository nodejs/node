'use strict';
require('../common');

Error.stackTraceLimit = 0;

console.error('before');

// Trigger stack overflow by stringifying a deeply nested array.
let array = [];
for (let i = 0; i < 100000; i++) {
  array = [ array ];
}

JSON.stringify(array);

console.error('after');
