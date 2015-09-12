'use strict';
var assert = require('assert');

Error.stackTraceLimit = 0;

console.error('before');

// stack overflow
function stackOverflow() {
  stackOverflow();
}
stackOverflow();

console.error('after');
