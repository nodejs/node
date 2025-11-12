'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const assert = require('assert');
const fs = require('fs');

const {
  HTTP2_HEADER_CONTENT_TYPE,
  HTTP2_HEADER_CONTENT_LENGTH
} = http2.constants;

const fname = fixtures.path('elipses.txt');
const data = fs.readFileSync(fname);
const stat = fs.statSync(fname);
fs.promises.open(fname, 'r').then(common.mustCall((fileHandle) => {
  const server = http2.createServer();
  server.on('stream', (stream) => {
    stream.respondWithFD(fileHandle, {
      [HTTP2_HEADER_CONTENT_TYPE]: 'text/plain',
      [HTTP2_HEADER_CONTENT_LENGTH]: stat.size,
    });
  });
  server.on('close', common.mustCall(() => fileHandle.close()));
  server.listen(0, common.mustCall(() => {

    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();

    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[HTTP2_HEADER_CONTENT_TYPE], 'text/plain');
      assert.strictEqual(+headers[HTTP2_HEADER_CONTENT_LENGTH], data.length);
    }));
    req.setEncoding('utf8');
    let check = '';
    req.on('data', (chunk) => check += chunk);
    req.on('end', common.mustCall(() => {
      assert.strictEqual(check, data.toString('utf8'));
      client.close();
      server.close();
    }));
    req.end();
  }));
}));
