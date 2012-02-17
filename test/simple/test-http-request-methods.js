// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');
var net = require('net');
var http = require('http');

// Test that the PATCH and PURGE verbs get passed through correctly

['PATCH', 'PURGE'].forEach(function(method, index) {
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
