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

// Verify that net.Server.listenFD() can be used to accept connections on an
// already-bound, already-listening socket.

var common = require('../common');
var assert = require('assert');
var http = require('http');
var netBinding = process.binding('net');

// Create an server and set it listening on a socket bound to common.PORT
var gotRequest = false;
var srv = http.createServer(function(req, resp) {
  gotRequest = true;

  resp.writeHead(200);
  resp.end();

  srv.close();
});

var fd = netBinding.socket('tcp4');
netBinding.bind(fd, common.PORT, '127.0.0.1');
netBinding.listen(fd, 128);
srv.listenFD(fd);

// Make an HTTP request to the server above
var hc = http.createClient(common.PORT, '127.0.0.1');
hc.request('/').end();

// Verify that we're exiting after having received an HTTP  request
process.addListener('exit', function() {
  assert.ok(gotRequest);
});
