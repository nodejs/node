// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const {
  HTTP2_HEADER_CONTENT_TYPE,
  HTTP2_HEADER_CONTENT_LENGTH,
  HTTP2_HEADER_LAST_MODIFIED
} = http2.constants;

const fname = path.resolve(common.fixturesDir, 'elipses.txt');
const data = fs.readFileSync(fname);
const stat = fs.statSync(fname);

const server = http2.createServer();
server.on('stream', (stream) => {
  stream.respondWithFile(fname, {
    [HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
  }, {
    statCheck(stat, headers) {
      headers[HTTP2_HEADER_LAST_MODIFIED] = stat.mtime.toUTCString();
      headers[HTTP2_HEADER_CONTENT_LENGTH] = stat.size;
    }
  });
});
server.listen(0, () => {

  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('response', common.mustCall((headers) => {
    assert.strictEqual(headers[HTTP2_HEADER_CONTENT_TYPE], 'text/plain');
    assert.strictEqual(+headers[HTTP2_HEADER_CONTENT_LENGTH], data.length);
    assert.strictEqual(headers[HTTP2_HEADER_LAST_MODIFIED],
                       stat.mtime.toUTCString());
  }));
  req.setEncoding('utf8');
  let check = '';
  req.on('data', (chunk) => check += chunk);
  req.on('end', common.mustCall(() => {
    assert.strictEqual(check, data.toString('utf8'));
    client.destroy();
    server.close();
  }));
  req.end();
});
