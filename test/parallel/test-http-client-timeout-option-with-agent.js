'use strict';

// Test that the request `timeout` option has precedence over the agent
// `timeout` option.

const { mustCall } = require('../common');
const { Agent, get } = require('http');
const { strictEqual } = require('assert');

const request = get({
  agent: new Agent({ timeout: 50 }),
  lookup: () => {},
  timeout: 100
});

request.on('socket', mustCall((socket) => {
  strictEqual(socket.timeout, 100);

  const listeners = socket.listeners('timeout');

  strictEqual(listeners.length, 1);
  strictEqual(listeners[0], request.timeoutCb);
}));
