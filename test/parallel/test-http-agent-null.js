'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

var request = 0;
var response = 0;
process.on('exit', function() {
  assert.equal(request, 1, 'http server "request" callback was not called');
  assert.equal(response, 1, 'http request "response" callback was not called');
});

var server = http.createServer(function(req, res) {
  request++;
  res.end();
}).listen(common.PORT, function() {
  var options = {
    agent: null,
    port: this.address().port
  };
  http.get(options, function(res) {
    response++;
    res.resume();
    server.close();
  });
});
