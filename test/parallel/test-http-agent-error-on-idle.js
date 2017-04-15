'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const Agent = http.Agent;

const server = http.createServer(function(req, res) {
  res.end('hello world');
});

server.listen(0, function() {
  const agent = new Agent({
    keepAlive: true,
  });

  const requestParams = {
    host: 'localhost',
    port: this.address().port,
    agent: agent,
    path: '/'
  };

  const socketKey = agent.getName(requestParams);

  get(function(res) {
    assert.strictEqual(res.statusCode, 200);
    res.resume();
    res.on('end', function() {
      process.nextTick(function() {
        const freeSockets = agent.freeSockets[socketKey];
        assert.strictEqual(freeSockets.length, 1,
                           'expect a free socket on ' + socketKey);

        //generate a random error on the free socket
        const freeSocket = freeSockets[0];
        freeSocket.emit('error', new Error('ECONNRESET: test'));

        get(done);
      });
    });
  });

  function get(callback) {
    return http.get(requestParams, callback);
  }

  function done() {
    assert.strictEqual(Object.keys(agent.freeSockets).length, 0,
                       'expect the freeSockets pool to be empty');

    agent.destroy();
    server.close();
    process.exit(0);
  }
});
