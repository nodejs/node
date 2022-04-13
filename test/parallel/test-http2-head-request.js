'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const errCheck = common.expectsError({
  name: 'Error',
  code: 'ERR_STREAM_WRITE_AFTER_END',
  message: 'write after end'
}, 1);

const {
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_STATUS,
  HTTP2_METHOD_HEAD,
} = http2.constants;

const server = http2.createServer();
server.on('stream', (stream, headers) => {

  assert.strictEqual(headers[HTTP2_HEADER_METHOD], HTTP2_METHOD_HEAD);

  stream.respond({ [HTTP2_HEADER_STATUS]: 200 });

  // Because this is a head request, the outbound stream is closed automatically
  stream.on('error', errCheck);
  stream.write('data');
});


server.listen(0, () => {

  const client = http2.connect(`http://localhost:${server.address().port}`);

  const req = client.request({
    [HTTP2_HEADER_METHOD]: HTTP2_METHOD_HEAD,
    [HTTP2_HEADER_PATH]: '/'
  });

  req.on('response', common.mustCall((headers, flags) => {
    assert.strictEqual(headers[HTTP2_HEADER_STATUS], 200);
    assert.strictEqual(flags, 5); // The end of stream flag is set
  }));
  req.on('data', common.mustNotCall());
  req.on('end', common.mustCall(() => {
    server.close();
    client.close();
  }));
});
