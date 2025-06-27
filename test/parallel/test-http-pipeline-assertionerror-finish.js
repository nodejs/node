'use strict';
const common = require('../common');

// This test ensures that Node.js doesn't crash with an AssertionError at
// `ServerResponse.resOnFinish` because of an out-of-order 'finish' bug in
// pipelining.
// https://github.com/nodejs/node/issues/2639

const http = require('http');
const net = require('net');

const COUNT = 10;

const server = http
  .createServer(
    common.mustCall((req, res) => {
      // Close the server, we have only one TCP connection anyway
      server.close();
      res.writeHead(200);
      res.write('data');

      setTimeout(function() {
        res.end();
      }, (Math.random() * 100) | 0);
    }, COUNT)
  )
  .listen(0, function() {
    const s = net.connect(this.address().port);

    const big = 'GET / HTTP/1.1\r\nHost: example.com\r\n\r\n'.repeat(COUNT);

    s.write(big);
    s.resume();
  });
