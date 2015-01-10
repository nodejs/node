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

if (process.platform === 'win32') {
  console.log('skipping test on windows, where clustered dgram is ENOTSUP');
  process.exit(0);
}

var cluster = require('cluster');
var dgram = require('dgram');

if (cluster.isMaster) {
  var unbound = cluster.fork().on('online', bind);

  function bind() {
    cluster.fork({BOUND: 'y'}).on('listening', disconnect);
  }

  function disconnect() {
    unbound.disconnect();
    unbound.on('disconnect', cluster.disconnect);
  }
} else {
  if (process.env.BOUND === 'y') {
    var source = dgram.createSocket('udp4');

    source.bind(0);
  }
}
