'use strict';
require('../common');
var assert = require('assert');
var http = require('http');
var Agent = http.Agent;

var server = http.createServer(function(req, res) {
  res.end('hello world');
});

server.listen(0, function() {
  var agent = new Agent({
    keepAlive: true,
  });

  var requestParams = {
    host: 'localhost',
    port: this.address().port,
    agent: agent,
    path: '/'
  };

  var socketKey = agent.getName(requestParams);

  get(function(res) {
    assert.equal(res.statusCode, 200);
    res.resume();
    res.on('end', function() {
      process.nextTick(function() {
        var freeSockets = agent.freeSockets[socketKey];
        assert.equal(freeSockets.length, 1,
                     'expect a free socket on ' + socketKey);

        //generate a random error on the free socket
        var freeSocket = freeSockets[0];
        freeSocket.emit('error', new Error('ECONNRESET: test'));

        get(done);
      });
    });
  });

  function get(callback) {
    return http.get(requestParams, callback);
  }

  function done() {
    assert.equal(Object.keys(agent.freeSockets).length, 0,
                 'expect the freeSockets pool to be empty');

    agent.destroy();
    server.close();
    process.exit(0);
  }
});
