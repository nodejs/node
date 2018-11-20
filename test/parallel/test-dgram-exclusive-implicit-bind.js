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
var cluster = require('cluster');
var dgram = require('dgram');

// Without an explicit bind, send() causes an implicit bind, which always
// generate a unique per-socket ephemeral port. An explicit bind to a port
// number causes all sockets bound to that number to share a port.
//
// The 2 workers that call bind() will share a port, the two workers that do
// not will not share a port, so master will see 3 unique source ports.

// Note that on Windows, clustered dgram is not supported. Since explicit
// binding causes the dgram to be clustered, don't fork the workers that bind.
// This is a useful test, still, because it demonstrates that by avoiding
// clustering, client (ephemeral, implicitly bound) dgram sockets become
// supported while using cluster, though servers still cause the master to error
// with ENOTSUP.

var windows = process.platform === 'win32';

if (cluster.isMaster) {
  var pass;
  var messages = 0;
  var ports = {};

  process.on('exit', function() {
    assert.equal(pass, true);
  });

  var target = dgram.createSocket('udp4');

  target.on('message', function(buf, rinfo) {
    messages++;
    ports[rinfo.port] = true;

    if (windows && messages === 2) {
      assert.equal(Object.keys(ports).length, 2);
      done();
    }

    if (!windows && messages === 4) {
      assert.equal(Object.keys(ports).length, 3);
      done();
    }

    function done() {
      pass = true;
      cluster.disconnect();
      target.close();
    }
  });

  target.on('listening', function() {
    cluster.fork();
    cluster.fork();
    if (!windows) {
      cluster.fork({BOUND: 'y'});
      cluster.fork({BOUND: 'y'});
    }
  });

  target.bind({port: common.PORT, exclusive: true});

  return;
}

var source = dgram.createSocket('udp4');

if (process.env.BOUND === 'y') {
  source.bind(0);
} else {
  // cluster doesn't know about exclusive sockets, so it won't close them. This
  // is expected, its the same situation for timers, outgoing tcp connections,
  // etc, which also keep workers alive after disconnect was requested.
  source.unref();
}

source.send(Buffer('abc'), 0, 3, common.PORT, '127.0.0.1');
