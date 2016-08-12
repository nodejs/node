'use strict';

require('../common');
const assert = require('assert');
const debug = require('_debugger');

function emit() {
  const error = new Error('sterrance');
  process.emit('uncaughtException', error);
}

assert.doesNotThrow(emit);

debug.start(['fhqwhgads']);

emit();
