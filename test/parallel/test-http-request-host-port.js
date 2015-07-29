'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

var connected = false;

const server = http.createServer(function(req, res) {
  connected = true;
  res.writeHead(204);
  res.end();
});

server.listen(common.PORT, function() {
 http.get({
    host: 'localhost:' + common.PORT
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
