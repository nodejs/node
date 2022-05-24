'use strict';
require('../common');
const assert = require('assert');
const EventEmitter = require('events');
const util = require('util');

const EE = new EventEmitter();

assert.throws(
  () => EE.emit('error', 'Accepts a string'),
  {
    code: 'ERR_UNHANDLED_ERROR',
    name: 'Error',
  }
);

assert.throws(
  () => EE.emit('error', { message: 'Error!' }),
  {
    code: 'ERR_UNHANDLED_ERROR',
    name: 'Error',
  }
);

assert.throws(
  () => EE.emit('error', {
    message: 'Error!',
    [util.inspect.custom]() { throw new Error(); }
  }),
  {
    code: 'ERR_UNHANDLED_ERROR',
    name: 'Error',
  }
);
