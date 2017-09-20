'use strict';

const common = require('../common');
const assert = require('assert');
const { get, createServer } = require('http');

// res.writable should not be set to false after it has finished sending
// Ref: https://github.com/nodejs/node/issues/15029

let external;

// Http server
const internal = createServer((req, res) => {
  res.writeHead(200);
  setImmediate(common.mustCall(() => {
    external.abort();
    res.end('Hello World\n');
  }));
}).listen(0);

// Proxy server
const server = createServer(common.mustCall((req, res) => {
  get(`http://127.0.0.1:${internal.address().port}`, common.mustCall((inner) => {
    res.on('close', common.mustCall(() => {
      assert.strictEqual(res.writable, true);
    }));
    inner.pipe(res);
  }));
})).listen(0, () => {
  external = get(`http://127.0.0.1:${server.address().port}`);
  external.on('error', common.mustCall((err) => {
    server.close();
    internal.close();
  }));
});
