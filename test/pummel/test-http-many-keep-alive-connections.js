'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const expected = 10000;
let responses = 0;
let requests = 0;
let connection;

const server = http.Server(function(req, res) {
  requests++;
  assert.strictEqual(req.connection, connection);
  res.writeHead(200);
  res.end('hello world\n');
});

server.once('connection', function(c) {
  connection = c;
});

server.listen(common.PORT, function connect() {
  const request = http.get({
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
  assert.strictEqual(expected, responses);
  assert.strictEqual(expected, requests);
});
