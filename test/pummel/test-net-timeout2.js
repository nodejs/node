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

// socket.write was not resetting the timeout timer. See
// https://github.com/joyent/node/issues/2002

var assert = require('assert');
var net = require('net');

var seconds = 5;
var gotTimeout = false;
var counter = 0;

var server = net.createServer(function(socket) {
  socket.setTimeout((seconds / 2) * 1000, function() {
    gotTimeout = true;
    console.log('timeout!!');
    socket.destroy();
    process.exit(1);
  });

  var interval = setInterval(function() {
    counter++;

    if (counter == seconds) {
      clearInterval(interval);
      server.close();
      socket.destroy();
    }

    if (socket.writable) {
      socket.write(Date.now() + '\n');
    }
  }, 1000);
});


server.listen(8888, function() {
  var s = net.connect(8888);
  s.pipe(process.stdout);
});


process.on('exit', function() {
  assert.equal(false, gotTimeout);
});
