'use strict';

require('../common');
const assert = require('assert');
const debug = require('_debugger');

function emit() {
  const error = new Error('fhqwhgads');
  process.emit('uncaughtException', error);
}

assert.doesNotThrow(emit);

// Send debug.start() an argv array of length 1 to avoid code that exits 
// if argv is empty.
debug.start(['sterrance']);

setImmediate(emit);
