'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');

const http2 = require('http2');
const assert = require('assert');
const fs = require('fs');

const {
  HTTP2_HEADER_CONTENT_TYPE,
  HTTP2_HEADER_CONTENT_LENGTH,
  HTTP2_HEADER_LAST_MODIFIED
} = http2.constants;

const fname = fixtures.path('elipses.txt');
const data = fs.readFileSync(fname);
const stat = fs.statSync(fname);
const fd = fs.openSync(fname, 'r');

const server = http2.createServer();
server.on('stream', (stream) => {
  stream.respond({});
  stream.end();

  stream.pushStream({
    ':path': '/file.txt',
    ':method': 'GET'
  }, (err, stream) => {
    assert.ifError(err);
    stream.respondWithFD(fd, {
      [HTTP2_HEADER_CONTENT_TYPE]: 'text/plain',
      [HTTP2_HEADER_CONTENT_LENGTH]: stat.size,
      [HTTP2_HEADER_LAST_MODIFIED]: stat.mtime.toUTCString()
    });
  });

  stream.end();
});

server.on('close', common.mustCall(() => fs.closeSync(fd)));

server.listen(0, () => {

  const client = http2.connect(`http://localhost:${server.address().port}`);

  let expected = 2;
  function maybeClose() {
    if (--expected === 0) {
      server.close();
      client.close();
    }
  }

  const req = client.request({});

  req.on('response', common.mustCall());

  client.on('stream', common.mustCall((stream, headers) => {

    stream.on('push', common.mustCall((headers) => {
      assert.strictEqual(headers[HTTP2_HEADER_CONTENT_TYPE], 'text/plain');
      assert.strictEqual(+headers[HTTP2_HEADER_CONTENT_LENGTH], data.length);
      assert.strictEqual(headers[HTTP2_HEADER_LAST_MODIFIED],
                         stat.mtime.toUTCString());
    }));

    stream.setEncoding('utf8');
    let check = '';
    stream.on('data', (chunk) => check += chunk);
    stream.on('end', common.mustCall(() => {
      assert.strictEqual(check, data.toString('utf8'));
      maybeClose();
    }));

  }));

  req.resume();
  req.on('end', maybeClose);

  req.end();
});
