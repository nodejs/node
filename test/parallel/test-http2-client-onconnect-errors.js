// Flags: --expose-http2
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
// - NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE (should emit session error)
// - NGHTTP2_ERR_INVALID_ARGUMENT (should emit stream error)
// - every other NGHTTP2 error from binding (should emit session error)

const specificTestKeys = [
  'NGHTTP2_ERR_NOMEM',
  'NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE',
  'NGHTTP2_ERR_INVALID_ARGUMENT'
];

const specificTests = [
  {
    ngError: constants.NGHTTP2_ERR_NOMEM,
    error: {
      code: 'ERR_OUTOFMEMORY',
      type: Error,
      message: 'Out of memory'
    },
    type: 'session'
  },
  {
    ngError: constants.NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE,
    error: {
      code: 'ERR_HTTP2_OUT_OF_STREAMS',
      type: Error,
      message: 'No stream ID is available because ' +
               'maximum stream ID has been reached'
    },
    type: 'session'
  },
  {
    ngError: constants.NGHTTP2_ERR_INVALID_ARGUMENT,
    error: {
      code: 'ERR_HTTP2_STREAM_SELF_DEPENDENCY',
      type: Error,
      message: 'A stream cannot depend on itself'
    },
    type: 'stream'
  },
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
    },
    type: 'session'
  }));

const tests = specificTests.concat(genericTests);

let currentError;

// mock submitRequest because we only care about testing error handling
Http2Session.prototype.submitRequest = () => currentError;

const server = http2.createServer(common.mustNotCall());

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

  currentError = test.ngError;
  req.resume();
  req.end();

  const errorMustCall = common.expectsError(test.error);
  const errorMustNotCall = common.mustNotCall(
    `${test.error.code} should emit on ${test.type}`
  );

  if (test.type === 'stream') {
    client.on('error', errorMustNotCall);
    req.on('error', errorMustCall);
    req.on('error', common.mustCall(() => {
      client.destroy();
    }));
  } else {
    client.on('error', errorMustCall);
    req.on('error', errorMustNotCall);
  }

  req.on('end', common.mustCall(() => {
    client.destroy();

    if (!tests.length) {
      server.close();
    } else {
      runTest(tests.shift());
    }
  }));
}
