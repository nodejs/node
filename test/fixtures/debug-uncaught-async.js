'use strict';

require('../common');
const assert = require('assert');
const debug = require('_debugger');

function emit() {
  const error = new Error('fhqwhgads');
  process.emit('uncaughtException', error);
}

assert.doesNotThrow(emit);

debug.start(['sterrance']);

setImmediate(emit);
