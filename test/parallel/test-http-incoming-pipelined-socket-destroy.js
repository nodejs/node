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

'use strict';
const common = require('../common');
const Countdown = require('../common/countdown');

const http = require('http');
const net = require('net');

const seeds = [ 3, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4 ];
const countdown = new Countdown(seeds.length, () => server.close());

// Set up some timing issues where sockets can be destroyed
// via either the req or res.
const server = http.createServer(common.mustCall(function(req, res) {
  switch (req.url) {
    case '/1':
      return setImmediate(function() {
        req.socket.destroy();
        server.emit('requestDone');
      });

    case '/2':
      return process.nextTick(function() {
        res.destroy();
        server.emit('requestDone');
      });

    // in one case, actually send a response in 2 chunks
    case '/3':
      res.write('hello ');
      return setImmediate(function() {
        res.end('world!');
        server.emit('requestDone');
      });

    default:
      res.destroy();
      server.emit('requestDone');
  }
}, seeds.length));


// Make a bunch of requests pipelined on the same socket
function generator(seeds) {
  const port = server.address().port;
  return seeds.map(function(r) {
    return `GET /${r} HTTP/1.1\r\n` +
           `Host: localhost:${port}\r\n` +
           '\r\n' +
           '\r\n';
  }).join('');
}


server.listen(0, common.mustCall(function() {
  const client = net.connect({ port: this.address().port });
  server.on('requestDone', function() {
    countdown.dec();
  });

  // immediately write the pipelined requests.
  // Some of these will not have a socket to destroy!
  client.write(generator(seeds));
}));
