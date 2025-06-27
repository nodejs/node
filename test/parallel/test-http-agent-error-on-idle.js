'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const Agent = http.Agent;

const server = http.createServer(common.mustCall((req, res) => {
  res.end('hello world');
}, 2));

server.listen(0, () => {
  const agent = new Agent({ keepAlive: true });

  const requestParams = {
    host: 'localhost',
    port: server.address().port,
    agent: agent,
    path: '/'
  };

  const socketKey = agent.getName(requestParams);

  http.get(requestParams, common.mustCall((res) => {
    assert.strictEqual(res.statusCode, 200);
    res.resume();
    res.on('end', common.mustCall(() => {
      process.nextTick(common.mustCall(() => {
        const freeSockets = agent.freeSockets[socketKey];
        // Expect a free socket on socketKey
        assert.strictEqual(freeSockets.length, 1);

        // Generate a random error on the free socket
        const freeSocket = freeSockets[0];
        freeSocket.emit('error', new Error('ECONNRESET: test'));

        http.get(requestParams, done);
      }));
    }));
  }));

  function done() {
    // Expect the freeSockets pool to be empty
    assert.strictEqual(Object.keys(agent.freeSockets).length, 0);

    agent.destroy();
    server.close();
  }
});
