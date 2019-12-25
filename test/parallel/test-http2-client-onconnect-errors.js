'use strict';
// Flags: --expose-internals

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { internalBinding } = require('internal/test/binding');
const {
  constants,
  Http2Session,
  nghttp2ErrorString
} = internalBinding('http2');
const http2 = require('http2');
const { NghttpError } = require('internal/http2/util');

// Tests error handling within requestOnConnect
// - NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE (should emit session error)
// - NGHTTP2_ERR_INVALID_ARGUMENT (should emit stream error)
// - every other NGHTTP2 error from binding (should emit session error)

const specificTestKeys = [
  'NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE',
  'NGHTTP2_ERR_INVALID_ARGUMENT'
];

const specificTests = [
  {
    ngError: constants.NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE,
    error: {
      code: 'ERR_HTTP2_OUT_OF_STREAMS',
      name: 'Error',
      message: 'No stream ID is available because ' +
               'maximum stream ID has been reached'
    },
    type: 'stream'
  },
  {
    ngError: constants.NGHTTP2_ERR_INVALID_ARGUMENT,
    error: {
      code: 'ERR_HTTP2_STREAM_SELF_DEPENDENCY',
      name: 'Error',
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
      constructor: NghttpError,
      name: 'Error',
      message: nghttp2ErrorString(constants[key])
    },
    type: 'session'
  }));

const tests = specificTests.concat(genericTests);

let currentError;

// Mock submitRequest because we only care about testing error handling
Http2Session.prototype.request = () => currentError;

const server = http2.createServer(common.mustNotCall());

server.listen(0, common.mustCall(() => runTest(tests.shift())));

function runTest(test) {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  client.on('close', common.mustCall());

  const req = client.request({ ':method': 'POST' });

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
  } else {
    client.on('error', errorMustCall);
    req.on('error', (err) => {
      common.expectsError({
        code: 'ERR_HTTP2_STREAM_CANCEL'
      })(err);
      common.expectsError({
        code: 'ERR_HTTP2_ERROR'
      })(err.cause);
    });
  }

  req.on('end', common.mustCall());
  req.on('close', common.mustCall(() => {
    client.destroy();

    if (!tests.length) {
      server.close();
    } else {
      runTest(tests.shift());
    }
  }));
}
