'use strict';
require('../common');

// This test ensures that `addRequest`'s Legacy API accepts `localAddress`
// correctly instead of accepting `path`.
// https://github.com/nodejs/node/issues/5051

const assert = require('assert');
const agent = require('http').globalAgent;

// Small stub just so we can call addRequest directly
const req = {
  getHeader: () => {}
};

agent.maxSockets = 0;

// localAddress is used when naming requests / sockets
// while using the Legacy API
// port 8080 is hardcoded since this does not create a network connection
agent.addRequest(req, 'localhost', 8080, '127.0.0.1');
assert.strictEqual(Object.keys(agent.requests).length, 1);
assert.strictEqual(
  Object.keys(agent.requests)[0],
  'localhost:8080:127.0.0.1');

// path is *not* used when naming requests / sockets
// port 8080 is hardcoded since this does not create a network connection
agent.addRequest(req, {
  host: 'localhost',
  port: 8080,
  localAddress: '127.0.0.1',
  path: '/foo'
});
assert.strictEqual(Object.keys(agent.requests).length, 1);
assert.strictEqual(
  Object.keys(agent.requests)[0],
  'localhost:8080:127.0.0.1');
