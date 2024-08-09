// Flags: --expose-internals --stack-size=3072
'use strict';
require('../common');
const assert = require('assert');
const { serializeError, deserializeError } = require('internal/error_serdes');

function cycle(err) {
  return deserializeError(serializeError(err));
}

class ErrorWithCyclicCause extends Error {
  get cause() {
    return new ErrorWithCyclicCause();
  }
}
const errorWithCyclicCause = Object
  .defineProperty(new Error('Error with cause'), 'cause', { get() { return errorWithCyclicCause; } });

assert.strictEqual(Object.hasOwn(cycle(errorWithCyclicCause), 'cause'), true);

// When the cause is cyclic, it is serialized until Maxiumum call stack size is reached
let depth = 0;
let e = cycle(new ErrorWithCyclicCause('Error with cause'));
while (e.cause) {
  e = e.cause;
  depth++;
}
assert(depth > 1);
console.log('Successfully completed stack exhaust test at', depth);
