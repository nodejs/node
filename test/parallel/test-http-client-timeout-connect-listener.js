'use strict';
const common = require('../common');

// This test ensures that `ClientRequest.prototype.setTimeout()` does
// not add a listener for the `'connect'` event to the socket if the
// socket is already connected.

const assert = require('assert');
const http = require('http');

// Maximum allowed value for timeouts.
const timeout = 2 ** 31 - 1;

const server = http.createServer((req, res) => {
  res.end();
});

server.listen(0, common.mustCall(() => {
  const agent = new http.Agent({ keepAlive: true, maxSockets: 1 });
  const options = { port: server.address().port, agent: agent };

  doRequest(options, common.mustCall(() => {
    const req = doRequest(options, common.mustCall(() => {
      agent.destroy();
      server.close();
    }));

    req.on('socket', common.mustCall((socket) => {
      assert.strictEqual(socket.listenerCount('connect'), 0);
    }));
  }));
}));

function doRequest(options, callback) {
  const req = http.get(options, (res) => {
    res.on('end', callback);
    res.resume();
  });

  req.setTimeout(timeout);
  return req;
}
