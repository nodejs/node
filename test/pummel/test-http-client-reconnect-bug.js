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

var net = require('net'),
    util = require('util'),
    http = require('http');

var errorCount = 0;
var eofCount = 0;

var server = net.createServer(function(socket) {
  socket.end();
});

server.on('listening', function() {
  var client = http.createClient(common.PORT);

  client.addListener('error', function(err) {
    console.log('ERROR! ' + err.message);
    errorCount++;
  });

  client.addListener('end', function() {
    console.log('EOF!');
    eofCount++;
  });

  var request = client.request('GET', '/', {'host': 'localhost'});
  request.end();
  request.addListener('response', function(response) {
    console.log('STATUS: ' + response.statusCode);
  });
});

server.listen(common.PORT);

setTimeout(function() {
  server.close();
}, 500);


process.addListener('exit', function() {
  assert.equal(1, errorCount);
  assert.equal(1, eofCount);
});
