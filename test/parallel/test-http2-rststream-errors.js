// Flags: --expose-http2
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

// tests error handling within rstStream
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
      type: Error,
      message: nghttp2ErrorString(constants[key])
    },
    type: 'stream'
  }));


const tests = specificTests.concat(genericTests);

let currentError;

// mock submitRstStream because we only care about testing error handling
Http2Stream.prototype.rstStream = () => currentError.ngError;

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
      stream.session.destroy();
    }));
  } else {
    stream.session.once('error', errorMustCall);
    stream.on('error', errorMustNotCall);
  }

  stream.rstStream();
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

  if (currentError.type === 'stream') {
    req.on('error', common.mustCall());
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
