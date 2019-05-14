'use strict';

const common = require('../common');
const assert = require('assert');
const { createServer } = require('http');
const { connect } = require('net');
const { finished } = require('stream');

// This test validates that the 'timeout' event fires
// after server.headersTimeout.

const headers =
  'GET / HTTP/1.1\r\n' +
  'Host: localhost\r\n' +
  'Agent: node\r\n';

const server = createServer(common.mustNotCall());
let sendCharEvery = 1000;

// 40 seconds is the default
assert.strictEqual(server.headersTimeout, 40 * 1000);

// Pass a REAL env variable to shortening up the default
// value which is 40s otherwise this is useful for manual
// testing
if (!process.env.REAL) {
  sendCharEvery = common.platformTimeout(10);
  server.headersTimeout = 2 * sendCharEvery;
}

server.once('timeout', common.mustCall((socket) => {
  socket.destroy();
}));

server.listen(0, common.mustCall(() => {
  const client = connect(server.address().port);
  client.write(headers);
  client.write('X-CRASH: ');

  const interval = setInterval(() => {
    client.write('a');
  }, sendCharEvery);

  client.resume();

  finished(client, common.mustCall((err) => {
    clearInterval(interval);
    server.close();
  }));
}));
