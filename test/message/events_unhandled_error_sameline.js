'use strict';
require('../common');
const EventEmitter = require('events');
new EventEmitter().emit('error', new Error());
