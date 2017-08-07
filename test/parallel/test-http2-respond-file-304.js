// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const assert = require('assert');
const path = require('path');

const {
  HTTP2_HEADER_CONTENT_TYPE,
  HTTP2_HEADER_STATUS
} = http2.constants;

const fname = path.resolve(common.fixturesDir, 'elipses.txt');

const server = http2.createServer();
server.on('stream', (stream) => {
  stream.respondWithFile(fname, {
    [HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
  }, {
    statCheck(stat, headers) {
      // abort the send and return a 304 Not Modified instead
      stream.respond({ [HTTP2_HEADER_STATUS]: 304 });
      return false;
    }
  });
});
server.listen(0, () => {

  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('response', common.mustCall((headers) => {
    assert.strictEqual(headers[HTTP2_HEADER_STATUS], 304);
    assert.strictEqual(headers[HTTP2_HEADER_CONTENT_TYPE, undefined]);
  }));

  req.on('data', common.mustNotCall());
  req.on('end', common.mustCall(() => {
    client.destroy();
    server.close();
  }));
  req.end();
});
