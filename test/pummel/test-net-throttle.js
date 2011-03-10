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

var N = 160 * 1024;
var chars_recved = 0;
var npauses = 0;

console.log('build big string');
var body = '';
for (var i = 0; i < N; i++) {
  body += 'C';
}

console.log('start server on port ' + common.PORT);

var server = net.createServer(function(connection) {
  connection.addListener('connect', function() {
    assert.equal(false, connection.write(body));
    console.log('bufferSize: ' + connection.bufferSize);
    assert.ok(0 <= connection.bufferSize &&
              connection.bufferSize <= N);
    connection.end();
  });
});

server.listen(common.PORT, function() {
  var paused = false;
  var client = net.createConnection(common.PORT);
  client.setEncoding('ascii');
  client.addListener('data', function(d) {
    chars_recved += d.length;
    console.log('got ' + chars_recved);
    if (!paused) {
      client.pause();
      npauses += 1;
      paused = true;
      console.log('pause');
      var x = chars_recved;
      setTimeout(function() {
        assert.equal(chars_recved, x);
        client.resume();
        console.log('resume');
        paused = false;
      }, 100);
    }
  });

  client.addListener('end', function() {
    server.close();
    client.end();
  });
});



process.addListener('exit', function() {
  assert.equal(N, chars_recved);
  assert.equal(true, npauses > 2);
});
