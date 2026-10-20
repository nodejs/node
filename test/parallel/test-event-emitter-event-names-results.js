'use strict';

require('../common');
const assert = require('node:assert');
const { EventEmitter } = require('node:events');

const emitter = new EventEmitter();
const listener = () => {};
const symbol = Symbol('event');
const keys = ['third', symbol, '2', '__proto__', 'first', '1'];
for (const key of keys) emitter.on(key, listener);

const expected = ['1', '2', 'third', '__proto__', 'first', symbol];
const result = emitter.eventNames();
assert.deepStrictEqual(result, expected);
result[0] = 'changed';
result.push('extra');
assert.deepStrictEqual(emitter.eventNames(), expected);

emitter.removeListener('2', listener);
emitter.removeListener('third', listener);
assert.deepStrictEqual(emitter.eventNames(), ['1', '__proto__', 'first', symbol]);
emitter.on('third', listener);
assert.deepStrictEqual(emitter.eventNames(), ['1', '__proto__', 'first', 'third', symbol]);
emitter.removeAllListeners();
assert.deepStrictEqual(emitter.eventNames(), []);
