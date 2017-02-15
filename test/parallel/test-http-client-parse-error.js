'use strict';
require('../common');
const assert = require('assert');

const http = require('http');
const net = require('net');

let connects = 0;
let parseErrors = 0;

// Create a TCP server
net.createServer(function(c) {
  console.log('connection');
  if (++connects === 1) {
    c.end('HTTP/1.1 302 Object Moved\r\nContent-Length: 0\r\n\r\nhi world');
  } else {
    c.end('bad http - should trigger parse error\r\n');
    this.close();
  }
}).listen(0, '127.0.0.1', function() {
  for (let i = 0; i < 2; i++) {
    http.request({
      host: '127.0.0.1',
      port: this.address().port,
      method: 'GET',
      path: '/'
    }).on('error', function(e) {
      console.log('got error from client');
      assert.ok(e.message.indexOf('Parse Error') >= 0);
      assert.strictEqual(e.code, 'HPE_INVALID_CONSTANT');
      parseErrors++;
    }).end();
  }
});

process.on('exit', function() {
  assert.strictEqual(connects, 2);
  assert.strictEqual(parseErrors, 2);
});
