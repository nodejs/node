'use strict';
require('../common');
const assert = require('assert');
const IncomingMessage = require('http').IncomingMessage;

let incomingMessage;

incomingMessage = new IncomingMessage();
assert.strictEqual(incomingMessage.bytesRead, 0);

incomingMessage = new IncomingMessage({bytesRead: 0});
assert.strictEqual(incomingMessage.bytesRead, 0);

incomingMessage = new IncomingMessage({bytesRead: 2});
assert.strictEqual(incomingMessage.bytesRead, 2);
