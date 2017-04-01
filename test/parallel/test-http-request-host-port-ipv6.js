'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

if (!common.hasIPv6) {
    console.log('1..0 # Skipped: no IPv6 support');
    return;
}

var connected = false;

const server = http.createServer(function(req, res) {
  connected = true;
  res.writeHead(204);
  res.end();
});

server.listen(common.PORT, '::1', function() {
 http.get({
    host: '[::1]:' + common.PORT
  }, function(res) {
    res.resume();
    server.close();
  }).on('error', function(e) {
    throw e;
  });
});

process.on('exit', function() {
  assert(connected, 'http.request should correctly parse ' +
      '{host: "hostname:port"} and connect to the specified host');
});
