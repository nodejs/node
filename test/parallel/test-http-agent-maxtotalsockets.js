'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const Countdown = require('../common/countdown');

const maxTotalSockets = 3;
const maxSockets = 2;

const agent = new http.Agent({
  keepAlive: true,
  keepAliveMsecs: 1000,
  maxTotalSockets,
  maxSockets,
  maxFreeSockets: 3
});

const server = http.createServer(common.mustCall((req, res) => {
  res.end('hello world');
}, 6));

server.keepAliveTimeout = 0;

const countdown = new Countdown(6, () => {
  assert.strictEqual(getRequestCount(), 0);
  agent.destroy();
  server.close();
});

function handler() {
  for (let i = 0; i < 6; i++) {
    http.get({
      host: 'localhost',
      port: server.address().port,
      agent: agent,
      path: `/${i}`,
      // Setting different origins
      family: i < 3 ? 4 : 6,
    }, common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 200);
      res.resume();
      res.on('end', common.mustCall(() => {
        testMaxSockets();
        assert(getTotalSocketsCount() <= maxTotalSockets);
        countdown.dec();
      }));
    }));
  }
}

function testMaxSockets() {
  for(const key of Object.keys(agent.sockets)) {
    assert(agent.sockets[key].length <= maxSockets)
  }
}

function getTotalSocketsCount() {
  let num = 0;
  for (const key of Object.keys(agent.sockets)) {
    num += agent.sockets[key].length;
  }
  return num;
}

function getRequestCount() {
  let num = 0;
  for (const key of Object.keys(agent.requests)) {
    num += agent.requests[key].length;
  }
  return num;
}

server.listen(0, common.mustCall(handler));
