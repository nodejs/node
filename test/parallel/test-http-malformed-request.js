'use strict';
require('../common');
var assert = require('assert');
var net = require('net');
var http = require('http');
var url = require('url');

// Make sure no exceptions are thrown when receiving malformed HTTP
// requests.

var nrequests_completed = 0;
var nrequests_expected = 1;

var server = http.createServer(function(req, res) {
  console.log('req: ' + JSON.stringify(url.parse(req.url)));

  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.write('Hello World');
  res.end();

  if (++nrequests_completed == nrequests_expected) server.close();
});
server.listen(0);

server.on('listening', function() {
  var c = net.createConnection(this.address().port);
  c.on('connect', function() {
    c.write('GET /hello?foo=%99bar HTTP/1.1\r\n\r\n');
    c.end();
  });
});

process.on('exit', function() {
  assert.equal(nrequests_expected, nrequests_completed);
});
