'use strict';

require('../common');
const assert = require('node:assert');
const { OutgoingMessage } = require('node:http');

const message = new OutgoingMessage();
assert.deepStrictEqual(message.getRawHeaderNames(), []);
assert.notStrictEqual(message.getRawHeaderNames(), message.getRawHeaderNames());

message.setHeader('X-First', 'first');
message.setHeader('10', 'ten');
message.setHeader('2', 'two');
message.setHeader('__proto__', 'proto');
message.setHeader('Constructor', 'constructor');
message.setHeader('X-Last', 'last');

const expected = ['2', '10', 'X-First', '__proto__', 'Constructor', 'X-Last'];
const names = message.getRawHeaderNames();
assert.deepStrictEqual(names, expected);
names[0] = 'changed';
names.push('extra');
assert.deepStrictEqual(message.getRawHeaderNames(), expected);
assert.strictEqual(message.getHeader('2'), 'two');

message.setHeader('x-FIRST', 'updated');
expected[2] = 'x-FIRST';
assert.deepStrictEqual(message.getRawHeaderNames(), expected);

message.removeHeader('X-FIRST');
message.setHeader('X-First', 'reinserted');
assert.deepStrictEqual(message.getRawHeaderNames(), [
  '2', '10', '__proto__', 'Constructor', 'X-Last', 'X-First',
]);

for (const name of message.getHeaderNames()) {
  message.removeHeader(name);
}
assert.deepStrictEqual(message.getRawHeaderNames(), []);
assert.notStrictEqual(message.getRawHeaderNames(), message.getRawHeaderNames());

message.setHeader('X-New', 'new');
assert.deepStrictEqual(message.getRawHeaderNames(), ['X-New']);
