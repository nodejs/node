'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var http = require('http');

// Test that the DELETE, PATCH and PURGE verbs get passed through correctly

['DELETE', 'PATCH', 'PURGE'].forEach(function(method, index) {
  var port = common.PORT + index;

  var server_response = '';
  var received_method = null;

  var server = http.createServer(function(req, res) {
    received_method = req.method;
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.write('hello ');
    res.write('world\n');
    res.end();
  });
  server.listen(port);

  server.on('listening', function() {
    var c = net.createConnection(port);

    c.setEncoding('utf8');

    c.on('connect', function() {
      c.write(method + ' / HTTP/1.0\r\n\r\n');
    });

    c.on('data', function(chunk) {
      console.log(chunk);
      server_response += chunk;
    });

    c.on('end', function() {
      c.end();
    });

    c.on('close', function() {
      server.close();
    });
  });

  process.on('exit', function() {
    var m = server_response.split('\r\n\r\n');
    assert.equal(m[1], 'hello world\n');
    assert.equal(received_method, method);
  });
});
