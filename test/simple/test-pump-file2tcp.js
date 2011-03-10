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
var fs = require('fs');
var util = require('util');
var path = require('path');
var fn = path.join(common.fixturesDir, 'elipses.txt');

var expected = fs.readFileSync(fn, 'utf8');

var server = net.createServer(function(stream) {
  common.error('pump!');
  util.pump(fs.createReadStream(fn), stream, function() {
    common.error('server stream close');
    common.error('server close');
    server.close();
  });
});

server.listen(common.PORT, function() {
  var conn = net.createConnection(common.PORT);
  conn.setEncoding('utf8');
  conn.addListener('data', function(chunk) {
    common.error('recv data! nchars = ' + chunk.length);
    buffer += chunk;
  });

  conn.addListener('end', function() {
    conn.end();
  });
  conn.addListener('close', function() {
    common.error('client connection close');
  });
});

var buffer = '';
var count = 0;

server.addListener('listening', function() {
});

process.addListener('exit', function() {
  assert.equal(expected, buffer);
});
