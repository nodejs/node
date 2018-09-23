'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

var create = 0;
var response = 0;
process.on('exit', function() {
  assert.equal(1, create, 'createConnection() http option was not called');
  assert.equal(1, response, 'http server "request" callback was not called');
});

var server = http.createServer(function(req, res) {
  res.end();
  response++;
}).listen(common.PORT, '127.0.0.1', function() {
  http.get({ createConnection: createConnection }, function(res) {
    res.resume();
    server.close();
  });
});

function createConnection() {
  create++;
  return net.createConnection(common.PORT, '127.0.0.1');
}
