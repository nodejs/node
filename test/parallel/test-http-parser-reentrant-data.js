// Flags: --expose-internals
'use strict';

// Data that a socket emits synchronously from inside a parser callback, for
// example by pushing into a Duplex, must be parsed after the current call
// to the parser returns, not from inside it.

const common = require('../common');
const assert = require('assert');
const http = require('http');
const { Duplex } = require('stream');
const { internalBinding } = require('internal/test/binding');
const { HTTPParser } = internalBinding('http_parser');

const execute = HTTPParser.prototype.execute;
let depth = 0;
HTTPParser.prototype.execute = common.mustCallAtLeast(function(...args) {
  assert.strictEqual(depth, 0);
  depth++;
  try {
    return execute.apply(this, args);
  } finally {
    depth--;
  }
}, 4);

{
  // Server: the request handler of the first request pushes the second one.
  const urls = [];
  let output = '';
  const connection = new Duplex({
    read() {},
    write(chunk, encoding, callback) {
      output += chunk;
      callback();
    },
  });
  const server = http.createServer(common.mustCall((req, res) => {
    urls.push(req.url);
    if (req.url === '/a')
      connection.push('GET /b HTTP/1.1\r\nHost: x\r\n\r\n');
    res.end('ok');
  }, 2));
  server.emit('connection', connection);
  connection.push('GET /a HTTP/1.1\r\nHost: x\r\n\r\n');
  setImmediate(common.mustCall(() => {
    assert.deepStrictEqual(urls, ['/a', '/b']);
    assert.strictEqual(output.split('HTTP/1.1 200 OK\r\n').length, 3);
  }));
}

{
  // Client: the 'information' handler pushes the final response.
  let socket;
  const req = http.request('http://localhost', {
    createConnection() {
      socket = new Duplex({
        read() {},
        write(chunk, encoding, callback) {
          if (chunk.toString().includes('\r\n\r\n'))
            this.push('HTTP/1.1 100 Continue\r\n\r\n');
          callback();
        },
      });
      return socket;
    },
  });
  req.on('information', common.mustCall(({ statusCode }) => {
    assert.strictEqual(statusCode, 100);
    socket.push('HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok');
    socket.push(null);
  }));
  req.on('response', common.mustCall((res) => {
    assert.strictEqual(res.statusCode, 200);
    res.setEncoding('utf8');
    let body = '';
    res.on('data', (chunk) => body += chunk);
    res.on('end', common.mustCall(() => assert.strictEqual(body, 'ok')));
  }));
  req.end();
}
