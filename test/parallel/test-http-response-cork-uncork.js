'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

let serverRes;

const server = http.createServer(common.mustCall((req, res) => {
  serverRes = res;
  res.writeHead(200);
  res.write('hello ');

  setImmediate(() => {
    res.cork();
    res.write('world');
  });
}));

server.listen(0, (err) => {
  assert.ifError(err);

  const req = http.request(server.address());
  req.end();

  req.on('response', common.mustCall((res) => {
    assert.strictEqual(res.statusCode, 200);

    res.setEncoding('utf8');
    res.once('data', common.mustCall((chunk) => {
      assert.strictEqual(chunk, 'hello ');
    }));

    setTimeout(() => {
      assert(serverRes);
      serverRes.uncork();

      res.once('data', common.mustCall((data) => {
        assert.strictEqual(data, 'world');
        serverRes.end();
        server.close();
      }));
    }, common.platformTimeout(100));
  }));
});
