'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

const MAX_COUNT = 2;

const server = http.createServer((req, res) => {
  const num = req.headers['x-num'];
  // TODO(@jasnell) At some point this should be refactored as the API
  // should not be allowing users to set multiple content-length values
  // in the first place.
  switch (num) {
    case '1':
      res.setHeader('content-length', [2, 1]);
      break;
    case '2':
      res.writeHead(200, { 'content-length': [1, 2] });
      break;
    default:
      assert.fail('should never get here');
  }
  res.end('ok');
});

let count = 0;

server.listen(0, common.mustCall(() => {
  for (let n = 1; n <= MAX_COUNT; n++) {
    // This runs twice, the first time, the server will use
    // setHeader, the second time it uses writeHead. In either
    // case, the error handler must be called because the client
    // is not allowed to accept multiple content-length headers.
    http.get(
      { port: server.address().port, headers: { 'x-num': n } },
      (res) => {
        assert.fail('client allowed multiple content-length headers.');
      }
    ).on('error', common.mustCall((err) => {
      assert(/^Parse Error/.test(err.message));
      assert.strictEqual(err.code, 'HPE_UNEXPECTED_CONTENT_LENGTH');
      count++;
      if (count === MAX_COUNT)
        server.close();
    }));
  }
}));

process.on('exit', () => {
  assert.strictEqual(count, MAX_COUNT);
});
