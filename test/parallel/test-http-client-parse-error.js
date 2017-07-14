'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');
const Countdown = require('../common/countdown');

const countdown = new Countdown(2, common.mustCall(() => server.close()));

const payloads = [
  'HTTP/1.1 302 Object Moved\r\nContent-Length: 0\r\n\r\nhi world',
  'bad http = should trigger parse error'
];

// Create a TCP server
const server =
  net.createServer(common.mustCall((c) => c.end(payloads.shift()), 2));

server.listen(0, common.mustCall(() => {
  for (let i = 0; i < 2; i++) {
    http.get({
      port: server.address().port,
      path: '/'
    }).on('error', common.mustCall((e) => {
      assert.ok(e.message.includes('Parse Error'));
      assert.strictEqual(e.code, 'HPE_INVALID_CONSTANT');
      countdown.dec();
    }));
  }
}));
