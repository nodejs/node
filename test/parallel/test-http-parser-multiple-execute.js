'use strict';

const common = require('../common');
const assert = require('assert');
const { request } = require('http');
const { Duplex } = require('stream');

let socket;

function createConnection(...args) {
  socket = new Duplex({
    read() {},
    write(chunk, encoding, callback) {
      if (chunk.toString().includes('\r\n\r\n')) {
        this.push('HTTP/1.1 100 Continue\r\n\r\n');
      }

      callback();
    }
  });

  return socket;
}

const req = request('http://localhost:8080', { createConnection });

req.on('information', common.mustCall(({ statusCode }) => {
  assert.strictEqual(statusCode, 100);
  socket.push('HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n');
  socket.push(null);
}));

req.on('response', common.mustCall(({ statusCode }) => {
  assert.strictEqual(statusCode, 200);
}));

req.end();
