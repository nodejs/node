'use strict';

// Test that the request `timeout` option has precedence over the agent
// `timeout` option.

const { mustCall } = require('../common');
const { Agent, get } = require('http');
const assert = require('assert');

const request = get({
  agent: new Agent({ timeout: 50 }),
  lookup: () => {},
  timeout: 100
});

request.on('socket', mustCall((socket) => {
  assert.strictEqual(socket.timeout, 100);

  const listeners = socket.listeners('timeout');

  assert.strictEqual(listeners.length, 2);
  assert.strictEqual(listeners[1], request.timeoutCb);
}));
