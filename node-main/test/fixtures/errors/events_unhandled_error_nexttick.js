'use strict';
require('../../common');
Error.stackTraceLimit = 1;

const EventEmitter = require('events');
const er = new Error();
process.nextTick(() => {
  new EventEmitter().emit('error', er);
});
