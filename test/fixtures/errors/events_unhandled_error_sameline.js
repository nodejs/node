'use strict';
require('../../common');
Error.stackTraceLimit = 1;

const EventEmitter = require('events');
new EventEmitter().emit('error', new Error());
