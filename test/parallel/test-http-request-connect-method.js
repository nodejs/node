// Flags: --expose-internals

'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const { validatePath } = require('internal/http');

const server = http.createServer();

server.on('connect', common.mustCall((req, stream) => {
  assert.strictEqual(req.url, 'example.com:80');
  stream.end('HTTP/1.1 501 Not Implemented\r\n\r\n');
}));

server.listen(0);

server.on('listening', common.mustCall(() => {
  const url = new URL(`http://localhost:${server.address().port}/example.com:80`);
  let req = http.request(url, { method: 'CONNECT' }).end();
  // invalid path
  const invalidPathURL = new URL(`http://localhost:${server.address().port}/example.com`);
  assert.throws(
    () => {
      req = http.request(invalidPathURL, { method: 'CONNECT' }).end();
    },
    {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
      message: /^The argument 'options\.path' must be a valid host:port combo\. Received .+$/
    }
  );
  req.once('connect', common.mustCall((res) => {
    res.destroy();
    server.close();
  }));
}));

['example.com', 'example.com:0', 'example.com:65536'].forEach((path) => {
  assert.throws(
    () => validatePath(path),
    {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
      message: /^The argument 'options\.path' must be a valid host:port combo\. Received .+$/
    }
  );
});
