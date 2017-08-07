// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const {
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_PATH,
  HTTP2_METHOD_POST,
  NGHTTP2_CANCEL,
  NGHTTP2_NO_ERROR,
  NGHTTP2_PROTOCOL_ERROR,
  NGHTTP2_REFUSED_STREAM,
  NGHTTP2_INTERNAL_ERROR
} = http2.constants;

const errCheck = common.expectsError({ code: 'ERR_HTTP2_STREAM_ERROR' }, 8);

function checkRstCode(rstMethod, expectRstCode) {
  const server = http2.createServer();
  server.on('stream', (stream, headers, flags) => {
    stream.respond({
      'content-type': 'text/html',
      ':status': 200
    });
    stream.write('test');
    if (rstMethod === 'rstStream')
      stream[rstMethod](expectRstCode);
    else
      stream[rstMethod]();

    if (expectRstCode > NGHTTP2_NO_ERROR) {
      stream.on('error', common.mustCall(errCheck));
    }
  });

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = http2.connect(`http://localhost:${port}`);

    const headers = {
      [HTTP2_HEADER_PATH]: '/',
      [HTTP2_HEADER_METHOD]: HTTP2_METHOD_POST
    };
    const req = client.request(headers);

    req.setEncoding('utf8');
    req.on('streamClosed', common.mustCall((actualRstCode) => {
      assert.strictEqual(
        expectRstCode, actualRstCode, `${rstMethod} is not match rstCode`);
      server.close();
      client.destroy();
    }));
    req.on('data', common.mustCall());
    req.on('aborted', common.mustCall());
    req.on('end', common.mustCall());

    if (expectRstCode > NGHTTP2_NO_ERROR) {
      req.on('error', common.mustCall(errCheck));
    }

  }));
}

checkRstCode('rstStream', NGHTTP2_NO_ERROR);
checkRstCode('rstWithNoError', NGHTTP2_NO_ERROR);
checkRstCode('rstWithProtocolError', NGHTTP2_PROTOCOL_ERROR);
checkRstCode('rstWithCancel', NGHTTP2_CANCEL);
checkRstCode('rstWithRefuse', NGHTTP2_REFUSED_STREAM);
checkRstCode('rstWithInternalError', NGHTTP2_INTERNAL_ERROR);
