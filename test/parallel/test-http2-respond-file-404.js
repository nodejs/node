// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const assert = require('assert');
const path = require('path');

const {
  HTTP2_HEADER_CONTENT_TYPE
} = http2.constants;

const server = http2.createServer();
server.on('stream', (stream) => {
  const file = path.join(process.cwd(), 'not-a-file');
  stream.respondWithFile(file, {
    [HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
  }, {
    onError(err) {
      common.expectsError({
        code: 'ENOENT',
        type: Error,
        message: `ENOENT: no such file or directory, open '${file}'`
      })(err);

      stream.respond({ ':status': 404 });
      stream.end();
    },
    statCheck: common.mustNotCall()
  });
});
server.listen(0, () => {

  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('response', common.mustCall((headers) => {
    assert.strictEqual(headers[':status'], 404);
  }));
  req.on('data', common.mustNotCall());
  req.on('end', common.mustCall(() => {
    client.destroy();
    server.close();
  }));
  req.end();
});
