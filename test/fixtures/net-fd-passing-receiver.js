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

process.mixin(require('../common'));
net = require('net');

path = process.ARGV[2];
greeting = process.ARGV[3];

receiver = net.createServer(function(socket) {
  socket.on('fd', function(fd) {
    var peerInfo = process.getpeername(fd);
    peerInfo.fd = fd;
    var passedSocket = new net.Socket(peerInfo);

    passedSocket.on('eof', function() {
      passedSocket.close();
    });

    passedSocket.on('data', function(data) {
      passedSocket.send('[echo] ' + data);
    });
    passedSocket.on('close', function() {
      receiver.close();
    });
    passedSocket.send('[greeting] ' + greeting);
  });
});

/* To signal the test runne we're up and listening */
receiver.on('listening', function() {
  common.print('ready');
});

receiver.listen(path);
