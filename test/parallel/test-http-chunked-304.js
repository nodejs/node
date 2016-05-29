'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

// RFC 2616, section 10.2.5:
//
//   The 204 response MUST NOT contain a message-body, and thus is always
//   terminated by the first empty line after the header fields.
//
// Likewise for 304 responses. Verify that no empty chunk is sent when
// the user explicitly sets a Transfer-Encoding header.

test(204, function() {
  test(304);
});

function test(statusCode, next) {
  var server = http.createServer(function(req, res) {
    res.writeHead(statusCode, { 'Transfer-Encoding': 'chunked' });
    res.end();
    server.close();
  });

  server.listen(0, function() {
    var conn = net.createConnection(this.address().port, function() {
      conn.write('GET / HTTP/1.1\r\n\r\n');

      var resp = '';
      conn.setEncoding('utf8');
      conn.on('data', function(data) {
        resp += data;
      });

      conn.on('end', common.mustCall(function() {
        assert.equal(/^Connection: close\r\n$/m.test(resp), true);
        assert.equal(/^0\r\n$/m.test(resp), false);
        if (next) process.nextTick(next);
      }));
    });
  });
}
