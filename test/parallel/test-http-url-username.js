'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const MakeDuplexPair = require('../common/duplexpair');

// Test that usernames from URLs are URL-decoded, as they should be.

{
  const url = new URL('http://localhost');
  url.username = 'test@test';
  url.password = '123456';

  const server = http.createServer(
    common.mustCall((req, res) => {
      assert.strictEqual(
        req.headers.authorization,
        'Basic ' + Buffer.from('test@test:123456').toString('base64'));
      res.statusCode = 200;
      res.end();
    }));

  const { clientSide, serverSide } = MakeDuplexPair();
  server.emit('connection', serverSide);

  const req = http.request(url, {
    createConnection: common.mustCall(() => clientSide)
  }, common.mustCall((res) => {
    res.resume();  // We don’t actually care about contents.
    res.on('end', common.mustCall());
  }));
  req.end();
}
