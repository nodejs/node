'use strict';

require('../common');
const EventEmitter = require('events');
const assert = require('assert');

const EE = new EventEmitter();
const m = () => {};
EE.on('foo', () => {});
assert.deepStrictEqual(['foo'], EE.listEvents());
EE.on('bar', m);
assert.deepStrictEqual(['foo', 'bar'], EE.listEvents());
EE.removeListener('bar', m);
assert.deepStrictEqual(['foo'], EE.listEvents());
const s = Symbol('s');
EE.on(s, m);
assert.deepStrictEqual(['foo', s], EE.listEvents());
EE.removeListener(s, m);
assert.deepStrictEqual(['foo'], EE.listEvents());
