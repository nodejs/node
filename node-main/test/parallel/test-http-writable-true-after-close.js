'use strict';

const common = require('../common');
const assert = require('assert');
const { get, createServer } = require('http');

// res.writable should not be set to false after it has finished sending
// Ref: https://github.com/nodejs/node/issues/15029

let internal;
let external;

// Proxy server
const server = createServer(common.mustCall((req, res) => {
  const listener = common.mustCall(() => {
    assert.strictEqual(res.writable, true);
  });

  // on CentOS 5, 'finish' is emitted
  res.on('finish', listener);
  // Everywhere else, 'close' is emitted
  res.on('close', listener);

  get(`http://127.0.0.1:${internal.address().port}`, common.mustCall((inner) => {
    inner.pipe(res);
  }));
})).listen(0, () => {
  // Http server
  internal = createServer((req, res) => {
    res.writeHead(200);
    setImmediate(common.mustCall(() => {
      external.abort();
      res.end('Hello World\n');
    }));
  }).listen(0, () => {
    external = get(`http://127.0.0.1:${server.address().port}`);
    external.on('error', common.mustCall(() => {
      server.close();
      internal.close();
    }));
  });
});
