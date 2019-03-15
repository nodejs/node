'use strict';
require('../common');
const assert = require('assert');
const { MessageChannel, drainMessagePort } = require('worker_threads');

const { port1, port2 } = new MessageChannel();
const messages = [];
port2.on('message', (message) => messages.push(message));

const message = { hello: 'world' };

assert.deepStrictEqual(messages, []);
port1.postMessage(message);
port1.postMessage(message);
assert.deepStrictEqual(messages, []);
drainMessagePort(port2);
assert.deepStrictEqual(messages, [ message, message ]);

port1.close();
