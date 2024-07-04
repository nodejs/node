'use strict';

const assert = require('assert');
const { CloseEvent, WebSocket, MessageEvent } = require('node:http');

assert.strictEqual(typeof WebSocket, 'function');
assert.strictEqual(typeof CloseEvent, 'function');
assert.strictEqual(typeof MessageEvent, 'function');

