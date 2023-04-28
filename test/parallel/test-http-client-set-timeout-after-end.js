'use strict';

// Test https://github.com/nodejs/node/issues/25499 fix.

const { mustCall } = require('../common');

const { Agent, createServer, get } = require('http');
const { strictEqual } = require('assert');

const server = createServer(mustCall((req, res) => {
  res.end();
}));

server.listen(0, () => {
  const agent = new Agent({ keepAlive: true, maxSockets: 1 });
  const port = server.address().port;

  let socket;

  const req = get({ agent, port }, (res) => {
    res.on('end', () => {
      strictEqual(req.setTimeout(0), req);
      strictEqual(socket.listenerCount('timeout'), 1);
      agent.destroy();
      server.close();
    });
    res.resume();
  });

  req.on('socket', (sock) => {
    socket = sock;
  });
});
