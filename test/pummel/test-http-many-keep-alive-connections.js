'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var expected = 10000;
var responses = 0;
var requests = 0;
var connection;

var server = http.Server(function(req, res) {
  requests++;
  assert.equal(req.connection, connection);
  res.writeHead(200);
  res.end('hello world\n');
});

server.once('connection', function(c) {
  connection = c;
});

server.listen(common.PORT, function connect() {
  var request = http.get({
    port: common.PORT,
    path: '/',
    headers: {
      'Connection': 'Keep-alive'
    }
  }, function(res) {
    res.on('end', function() {
      if (++responses < expected) {
        connect();
      } else {
        server.close();
      }
    });
    res.resume();
  }).on('error', function(e) {
    console.log(e.message);
    process.exit(1);
  });
  request.agent.maxSockets = 1;
});

process.on('exit', function() {
  assert.equal(expected, responses);
  assert.equal(expected, requests);
});
