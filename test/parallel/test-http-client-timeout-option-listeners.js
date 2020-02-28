'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');

const agent = new http.Agent({ keepAlive: true });

const server = http.createServer((req, res) => {
  res.end('');
});

// Maximum allowed value for timeouts
const timeout = 2 ** 31 - 1;

const options = {
  agent,
  method: 'GET',
  port: undefined,
  host: common.localhostIPv4,
  path: '/',
  timeout: timeout
};

server.listen(0, options.host, common.mustCall(() => {
  options.port = server.address().port;
  doRequest(common.mustCall((numListeners) => {
    assert.strictEqual(numListeners, 2);
    doRequest(common.mustCall((numListeners) => {
      assert.strictEqual(numListeners, 2);
      server.close();
      agent.destroy();
    }));
  }));
}));

function doRequest(cb) {
  http.request(options, common.mustCall((response) => {
    const sockets = agent.sockets[`${options.host}:${options.port}:`];
    assert.strictEqual(sockets.length, 1);
    const socket = sockets[0];
    const numListeners = socket.listeners('timeout').length;
    response.resume();
    response.once('end', common.mustCall(() => {
      process.nextTick(cb, numListeners);
    }));
  })).end();
}
