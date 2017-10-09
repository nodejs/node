'use strict';

const {
  constants,
  Http2Session,
  nghttp2ErrorString
} = process.binding('http2');
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

// tests error handling within requestOnConnect
// - NGHTTP2_ERR_NOMEM (should emit session error)
// - every other NGHTTP2 error from binding (should emit session error)

const specificTestKeys = [
  'NGHTTP2_ERR_NOMEM'
];

const specificTests = [
  {
    ngError: constants.NGHTTP2_ERR_NOMEM,
    error: {
      code: 'ERR_OUTOFMEMORY',
      type: Error,
      message: 'Out of memory'
    }
  }
];

const genericTests = Object.getOwnPropertyNames(constants)
  .filter((key) => (
    key.indexOf('NGHTTP2_ERR') === 0 && specificTestKeys.indexOf(key) < 0
  ))
  .map((key) => ({
    ngError: constants[key],
    error: {
      code: 'ERR_HTTP2_ERROR',
      type: Error,
      message: nghttp2ErrorString(constants[key])
    }
  }));

const tests = specificTests.concat(genericTests);

const server = http2.createServer(common.mustNotCall());
server.on('sessionError', () => {}); // not being tested

server.listen(0, common.mustCall(() => runTest(tests.shift())));

function runTest(test) {
  // mock submitSettings because we only care about testing error handling
  Http2Session.prototype.submitSettings = () => test.ngError;

  const errorMustCall = common.expectsError(test.error);
  const errorMustNotCall = common.mustNotCall(
    `${test.error.code} should emit on session`
  );

  const url = `http://localhost:${server.address().port}`;

  const client = http2.connect(url, {
    settings: {
      maxHeaderListSize: 1
    }
  });

  const req = client.request();
  req.resume();
  req.end();

  client.on('error', errorMustCall);
  req.on('error', errorMustNotCall);

  req.on('end', common.mustCall(() => {
    client.destroy();
    if (!tests.length) {
      server.close();
    } else {
      runTest(tests.shift());
    }
  }));
}
