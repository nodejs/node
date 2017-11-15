'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const {
  constants,
  Http2Stream,
  nghttp2ErrorString
} = process.binding('http2');

// tests error handling within pushStream
// - NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE (should emit session error)
// - NGHTTP2_ERR_STREAM_CLOSED (should emit stream error)
// - every other NGHTTP2 error from binding (should emit stream error)

const specificTestKeys = [
  'NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE',
  'NGHTTP2_ERR_STREAM_CLOSED'
];

const specificTests = [
  {
    ngError: constants.NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE,
    error: {
      code: 'ERR_HTTP2_OUT_OF_STREAMS',
      type: Error,
      message: 'No stream ID is available because ' +
               'maximum stream ID has been reached'
    },
    type: 'stream'
  },
  {
    ngError: constants.NGHTTP2_ERR_STREAM_CLOSED,
    error: {
      code: 'ERR_HTTP2_STREAM_CLOSED',
      type: Error,
      message: 'The stream is already closed'
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
    type: 'stream'
  }));


const tests = specificTests.concat(genericTests);

let currentError;

// mock submitPushPromise because we only care about testing error handling
Http2Stream.prototype.pushPromise = () => currentError.ngError;

const server = http2.createServer();
server.on('stream', common.mustCall((stream, headers) => {
  const errorMustCall = common.expectsError(currentError.error);
  const errorMustNotCall = common.mustNotCall(
    `${currentError.error.code} should emit on ${currentError.type}`
  );

  if (currentError.type === 'stream') {
    stream.session.on('error', errorMustNotCall);
    stream.on('error', errorMustCall);
    stream.on('error', common.mustCall(() => {
      stream.respond();
      stream.end();
    }));
  } else {
    stream.session.once('error', errorMustCall);
    stream.on('error', errorMustNotCall);
  }

  stream.pushStream({}, () => {});
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
