'use strict';
const common = require('../common');
const assert = require('assert');
const agent = require('http').globalAgent;

// small stub just so we can call addRequest directly
const req = {
  getHeader: function() {}
};

agent.maxSockets = 0;

// localAddress is used when naming requests / sockets
// while using the Legacy API
agent.addRequest(req, 'localhost', common.PORT, '127.0.0.1');
assert.equal(Object.keys(agent.requests).length, 1);
assert.equal(
  Object.keys(agent.requests)[0],
  'localhost:' + common.PORT + ':127.0.0.1');

// path is *not* used when naming requests / sockets
agent.addRequest(req, {
  host: 'localhost',
  port: common.PORT,
  localAddress: '127.0.0.1',
  path: '/foo'
});
assert.equal(Object.keys(agent.requests).length, 1);
assert.equal(
  Object.keys(agent.requests)[0],
  'localhost:' + common.PORT + ':127.0.0.1');
