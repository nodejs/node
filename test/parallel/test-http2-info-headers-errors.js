'use strict';
// Flags: --expose-internals

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const { internalBinding } = require('internal/test/binding');
const {
  constants,
  Http2Stream,
  nghttp2ErrorString
} = internalBinding('http2');
const { NghttpError } = require('internal/http2/util');

// Tests error handling within additionalHeaders
// - every other NGHTTP2 error from binding (should emit stream error)

const specificTestKeys = [];
const specificTests = [];

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
    type: 'stream'
  }));


const tests = specificTests.concat(genericTests);

let currentError;

// Mock sendHeaders because we only care about testing error handling
Http2Stream.prototype.info = () => currentError.ngError;

const server = http2.createServer();
server.on('stream', common.mustCall((stream, headers) => {
  const errorMustCall = common.expectsError(currentError.error);
  const errorMustNotCall = common.mustNotCall(
    `${currentError.error.code} should emit on ${currentError.type}`
  );

  if (currentError.type === 'stream') {
    stream.session.on('error', errorMustNotCall);
    stream.on('error', errorMustCall);
  } else {
    stream.session.once('error', errorMustCall);
    stream.on('error', errorMustNotCall);
  }

  stream.additionalHeaders({ ':status': 100 });
}, tests.length));

server.listen(0, common.mustCall(() => runTest(tests.shift())));

function runTest(test) {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request({ ':method': 'POST' });

  currentError = test;
  req.resume();
  req.end();

  req.on('error', common.expectsError({
    code: 'ERR_HTTP2_STREAM_ERROR',
    name: 'Error',
    message: 'Stream closed with error code NGHTTP2_INTERNAL_ERROR'
  }));

  req.on('close', common.mustCall(() => {
    client.close();

    if (!tests.length) {
      server.close();
    } else {
      runTest(tests.shift());
    }
  }));
}
