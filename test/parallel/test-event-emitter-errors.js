'use strict';
const common = require('../common');
const EventEmitter = require('events');

const EE = new EventEmitter();

common.expectsError(
  () => EE.emit('error', 'Accepts a string'),
  {
    code: 'ERR_UNHANDLED_ERROR',
    type: Error,
    message: 'Unhandled error. (Accepts a string)'
  }
);

common.expectsError(
  () => EE.emit('error', { message: 'Error!' }),
  {
    code: 'ERR_UNHANDLED_ERROR',
    type: Error,
    message: 'Unhandled error. ([object Object])'
  }
);

// Verify that the event type cannot be undefined.
common.expectsError(() => {
  EE.emit(undefined, new Error());
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "eventName" argument must be one of type string or symbol. ' +
           'Received type undefined'
});
