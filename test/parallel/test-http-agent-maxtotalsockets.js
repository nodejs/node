'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const Countdown = require('../common/countdown');

assert.throws(() => new http.Agent({
  maxTotalSockets: 'test',
}), {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
  message: 'The "maxTotalSockets" argument must be of type number. ' +
    "Received type string ('test')",
});

[NaN, Infinity].forEach((item) => {
  assert.throws(() => new http.Agent({
    maxTotalSockets: item,
  }), {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "maxTotalSockets" is out of range. ' +
      `It must be an integer. Received ${item}`,
  });
});

[-1, 0].forEach((item) => {
  assert.throws(() => new http.Agent({
    maxTotalSockets: item,
  }), {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "maxTotalSockets" is out of range. ' +
      `It must be > 0. Received ${item}`,
  });
});

const maxTotalSockets = 2;
const maxSockets = 3;

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
const server2 = http.createServer(common.mustCall((req, res) => {
  res.end('hello world');
}, 6));

server.keepAliveTimeout = 0;
server2.keepAliveTimeout = 0;

const countdown = new Countdown(12, () => {
  assert.strictEqual(getRequestCount(), 0);
  agent.destroy();
  server.close();
  server2.close();
});

function handler(s) {
  for (let i = 0; i < 6; i++) {
    http.get({
      host: 'localhost',
      port: s.address().port,
      agent,
      path: `/${i}`,
    }, common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 200);
      res.resume();
      res.on('end', common.mustCall(() => {
        for (const key of Object.keys(agent.sockets)) {
          assert(agent.sockets[key].length <= maxSockets);
        }
        const totalSocketsCount = getTotalSocketsCount();
        assert(totalSocketsCount <= maxTotalSockets);
        countdown.dec();
      }));
    }));
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

server.listen(0, common.mustCall(() => handler(server)));
server2.listen(0, common.mustCall(() => handler(server2)));
