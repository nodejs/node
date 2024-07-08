'use strict';

require('../common');
const assert = require('assert');
const {
  WebSocket: NodeHttpWebSocket,
  MessageEvent: NodeHttpMessageEvent
} = require('node:http');

// Compare with global objects
assert.strictEqual(NodeHttpWebSocket, WebSocket);
assert.strictEqual(NodeHttpMessageEvent, MessageEvent);
