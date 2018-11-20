'use strict';
const { mustCall } = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

// https://github.com/nodejs/node/issues/17789 - a connection upgrade response
// that has a Transfer-Encoding header and a body whose first byte is > 127
// triggers a bug where said byte is skipped over.
net.createServer(mustCall(function(conn) {
  conn.write('HTTP/1.1 101 Switching Protocols\r\n' +
             'Connection: upgrade\r\n' +
             'Transfer-Encoding: chunked\r\n' +
             'Upgrade: websocket\r\n' +
             '\r\n' +
             '\u0080', 'latin1');
  this.close();
})).listen(0, mustCall(function() {
  http.get({
    host: this.address().host,
    port: this.address().port,
    headers: { 'Connection': 'upgrade', 'Upgrade': 'websocket' },
  }).on('upgrade', mustCall((res, conn, head) => {
    assert.strictEqual(head.length, 1);
    assert.strictEqual(head[0], 128);
    conn.destroy();
  }));
}));
