'use strict';

require('../common');
const EventEmitter = require('events');
const assert = require('assert');

const EE = new EventEmitter();
const m = () => {};
EE.on('foo', () => {});
assert.deepStrictEqual(['foo'], EE.eventNames());
EE.on('bar', m);
assert.deepStrictEqual(['foo', 'bar'], EE.eventNames());
EE.removeListener('bar', m);
assert.deepStrictEqual(['foo'], EE.eventNames());
const s = Symbol('s');
EE.on(s, m);
assert.deepStrictEqual(['foo', s], EE.eventNames());
EE.removeListener(s, m);
assert.deepStrictEqual(['foo'], EE.eventNames());
