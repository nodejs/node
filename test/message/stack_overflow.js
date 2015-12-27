'use strict';
require('../common');

Error.stackTraceLimit = 0;

console.error('before');

// stack overflow
function stackOverflow() {
  stackOverflow();
}
stackOverflow();

console.error('after');
