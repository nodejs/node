'use strict';
const common = require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

var options = {
  host: '127.0.0.1',
  port: undefined
};

// start a tcp server that closes incoming connections immediately
var server = net.createServer(function(client) {
  client.destroy();
  server.close();
});
server.listen(0, options.host, common.mustCall(onListen));

// do a GET request, expect it to fail
function onListen() {
  options.port = this.address().port;
  var req = http.request(options, function(res) {
    assert.ok(false, 'this should never run');
  });
  req.on('error', common.mustCall(function(err) {
    assert.equal(err.code, 'ECONNRESET');
  }));
  req.end();
}
