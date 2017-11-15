// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const {
  constants,
  Http2Session,
  nghttp2ErrorString
} = process.binding('http2');

// tests error handling within shutdown
// - should emit ERR_HTTP2_ERROR on session for all errors

const tests = Object.getOwnPropertyNames(constants)
  .filter((key) => (
    key.indexOf('NGHTTP2_ERR') === 0
  ))
  .map((key) => ({
    ngError: constants[key],
    error: {
      code: 'ERR_HTTP2_ERROR',
      type: Error,
      message: nghttp2ErrorString(constants[key])
    }
  }));

let currentError;

// mock submitGoaway because we only care about testing error handling
Http2Session.prototype.goaway = () => currentError.ngError;

const server = http2.createServer();
server.on('stream', common.mustCall((stream, headers) => {
  const errorMustCall = common.expectsError(currentError.error);
  const errorMustNotCall = common.mustNotCall(
    `${currentError.error.code} should emit on session`
  );

  stream.session.once('error', errorMustCall);
  stream.on('error', errorMustNotCall);

  stream.session.shutdown();
}, tests.length));

server.listen(0, common.mustCall(() => runTest(tests.shift())));

function runTest(test) {
  const port = server.address().port;
  const url = `http://localhost:${port}`;
  const headers = {
    ':path': '/',
    ':method': 'POST',
    ':scheme': 'http',
    ':authority': `localhost:${port}`
  };

  const client = http2.connect(url);
  const req = client.request(headers);

  currentError = test;
  req.resume();
  req.end();

  req.on('end', common.mustCall(() => {
    client.destroy();

    if (!tests.length) {
      server.close();
    } else {
      runTest(tests.shift());
    }
  }));
}
