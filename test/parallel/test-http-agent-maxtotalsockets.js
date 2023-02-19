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

[-1, 0, NaN].forEach((item) => {
  assert.throws(() => new http.Agent({
    maxTotalSockets: item,
  }), {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  });
});

assert.ok(new http.Agent({
  maxTotalSockets: Infinity,
}));

function start(param = {}) {
  const { maxTotalSockets, maxSockets } = param;

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
          assert(getTotalSocketsCount() <= maxTotalSockets);
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
}

// If maxTotalSockets is larger than maxSockets,
// then the origin check will be skipped
// when the socket is removed.
[{
  maxTotalSockets: 2,
  maxSockets: 3,
}, {
  maxTotalSockets: 3,
  maxSockets: 2,
}, {
  maxTotalSockets: 2,
  maxSockets: 2,
}].forEach(start);
