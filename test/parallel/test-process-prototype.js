'use strict';
require('../common');
const assert = require('assert');
const EventEmitter = require('events');

const proto = Object.getPrototypeOf(process);

assert(proto instanceof EventEmitter);

const desc = Object.getOwnPropertyDescriptor(proto, 'constructor');

assert.strictEqual(desc.value, process.constructor);
assert.strictEqual(desc.writable, true);
assert.strictEqual(desc.enumerable, false);
assert.strictEqual(desc.configurable, true);
