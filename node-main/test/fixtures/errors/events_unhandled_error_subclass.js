'use strict';
require('../../common');
Error.stackTraceLimit = 1;

const EventEmitter = require('events');
class Foo extends EventEmitter {}
new Foo().emit('error', new Error());
