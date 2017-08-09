// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();
const data = Buffer.from([0x1, 0x2, 0x3, 0x4, 0x5]);

server.on('stream', common.mustCall((stream) => {
  stream.session.shutdown({
    errorCode: 1,
    opaqueData: data
  });
  stream.end();
  stream.on('error', common.mustCall(common.expectsError({
    code: 'ERR_HTTP2_STREAM_ERROR',
    type: Error,
    message: 'Stream closed with error code 7'
  })));
}));

server.listen(0, () => {

  const client = http2.connect(`http://localhost:${server.address().port}`);
  client.on('goaway', common.mustCall((code, lastStreamID, buf) => {
    assert.deepStrictEqual(code, 1);
    assert.deepStrictEqual(lastStreamID, 0);
    assert.deepStrictEqual(data, buf);
    server.close();
  }));
  const req = client.request({ ':path': '/' });
  req.resume();
  req.on('end', common.mustCall());
  req.end();

});
