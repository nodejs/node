'use strict';
const common = require('../common');
const assert = require('assert');
const { MessageChannel, receiveMessageOnPort } = require('worker_threads');

const { port1, port2 } = new MessageChannel();

const message1 = { hello: 'world' };
const message2 = { foo: 'bar' };

// Make sure receiveMessageOnPort() works in a FIFO way, the same way it does
// when we’re using events.
assert.deepStrictEqual(receiveMessageOnPort(port2), undefined);
port1.postMessage(message1);
port1.postMessage(message2);
assert.deepStrictEqual(receiveMessageOnPort(port2), { message: message1 });
assert.deepStrictEqual(receiveMessageOnPort(port2), { message: message2 });
assert.deepStrictEqual(receiveMessageOnPort(port2), undefined);
assert.deepStrictEqual(receiveMessageOnPort(port2), undefined);

// Make sure message handlers aren’t called.
port2.on('message', common.mustNotCall());
port1.postMessage(message1);
assert.deepStrictEqual(receiveMessageOnPort(port2), { message: message1 });
port1.close();

for (const value of [null, 0, -1, {}, []]) {
  assert.throws(() => receiveMessageOnPort(value), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'First argument needs to be a MessagePort instance'
  });
}
