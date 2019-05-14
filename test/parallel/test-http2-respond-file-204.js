'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const http2 = require('http2');

const {
  HTTP2_HEADER_CONTENT_TYPE,
  HTTP2_HEADER_STATUS
} = http2.constants;

const fname = fixtures.path('elipses.txt');

const server = http2.createServer();
server.on('stream', (stream) => {
  common.expectsError(() => {
    stream.respondWithFile(fname, {
      [HTTP2_HEADER_STATUS]: 204,
      [HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
    });
  }, {
    code: 'ERR_HTTP2_PAYLOAD_FORBIDDEN',
    type: Error,
    message: 'Responses with 204 status must not have a payload'
  });
  stream.respond({});
  stream.end();
});
server.listen(0, () => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();
  req.on('response', common.mustCall());
  req.on('data', common.mustNotCall());
  req.on('end', common.mustCall(() => {
    client.close();
    server.close();
  }));
  req.end();
});
