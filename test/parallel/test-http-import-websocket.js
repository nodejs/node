'use strict';

require('../common');
const assert = require('assert');
const { WebSocket: NodeHttpWebSocket, CloseEvent: NodeHttpCloseEvent, MessageEvent: NodeHttpMessageEvent } = require('node:http');

assert.strictEqual(typeof NodeHttpWebSocket, 'function');
assert.strictEqual(typeof NodeHttpCloseEvent, 'function');
assert.strictEqual(typeof NodeHttpMessageEvent, 'function');

// compare with global objects
assert.strictEqual(NodeHttpWebSocket, WebSocket);
assert.strictEqual(NodeHttpCloseEvent, CloseEvent);
assert.strictEqual(NodeHttpMessageEvent, MessageEvent);
