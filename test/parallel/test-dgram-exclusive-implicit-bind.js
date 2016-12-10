'use strict';
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

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const dgram = require('dgram');

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

if (cluster.isMaster) {
  var messages = 0;
  const ports = {};
  const pids = [];

  var target = dgram.createSocket('udp4');

  const done = common.mustCall(function() {
    cluster.disconnect();
    target.close();
  });

  target.on('message', function(buf, rinfo) {
    if (pids.includes(buf.toString()))
      return;
    pids.push(buf.toString());
    messages++;
    ports[rinfo.port] = true;

    if (common.isWindows && messages === 2) {
      assert.strictEqual(Object.keys(ports).length, 2);
      done();
    }

    if (!common.isWindows && messages === 4) {
      assert.strictEqual(Object.keys(ports).length, 3);
      done();
    }
  });

  target.on('listening', function() {
    cluster.fork();
    cluster.fork();
    if (!common.isWindows) {
      cluster.fork({BOUND: 'y'});
      cluster.fork({BOUND: 'y'});
    }
  });

  target.bind({port: common.PORT, exclusive: true});

  return;
}

const source = dgram.createSocket('udp4');
var interval;

source.on('close', function() {
  clearInterval(interval);
});

if (process.env.BOUND === 'y') {
  source.bind(0);
} else {
  // cluster doesn't know about exclusive sockets, so it won't close them. This
  // is expected, its the same situation for timers, outgoing tcp connections,
  // etc, which also keep workers alive after disconnect was requested.
  source.unref();
}

const buf = Buffer.from(process.pid.toString());
interval = setInterval(() => {
  source.send(buf, common.PORT, '127.0.0.1');
}, 1).unref();
