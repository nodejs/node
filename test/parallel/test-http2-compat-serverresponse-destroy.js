// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

// Check that destroying the Http2ServerResponse stream produces
// the expected result, including the ability to throw an error
// which is emitted on server.streamError

const errors = [
  'test-error',
  Error('test')
];
let nextError;

const server = http2.createServer(common.mustCall((req, res) => {
  req.on('error', common.mustNotCall());
  res.on('error', common.mustNotCall());

  res.on('finish', common.mustCall(() => {
    assert.doesNotThrow(() => res.destroy(nextError));
    assert.strictEqual(res.closed, true);
  }));

  if (req.path !== '/') {
    nextError = errors.shift();
  }
  res.destroy(nextError);
}, 3));

server.on(
  'streamError',
  common.mustCall((err) => assert.strictEqual(err, nextError), 2)
);

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);
  const req = client.request({
    ':path': '/',
    ':method': 'GET',
    ':scheme': 'http',
    ':authority': `localhost:${port}`
  });

  req.on('response', common.mustNotCall());
  req.on('error', common.mustNotCall());
  req.on('end', common.mustCall());

  req.resume();
  req.end();

  const req2 = client.request({
    ':path': '/error',
    ':method': 'GET',
    ':scheme': 'http',
    ':authority': `localhost:${port}`
  });

  req2.on('response', common.mustNotCall());
  req2.on('error', common.mustNotCall());
  req2.on('end', common.mustCall());

  req2.resume();
  req2.end();

  const req3 = client.request({
    ':path': '/error',
    ':method': 'GET',
    ':scheme': 'http',
    ':authority': `localhost:${port}`
  });

  req3.on('response', common.mustNotCall());
  req3.on('error', common.expectsError({
    code: 'ERR_HTTP2_STREAM_ERROR',
    type: Error,
    message: 'Stream closed with error code 2'
  }));
  req3.on('end', common.mustCall(() => {
    server.close();
    client.destroy();
  }));

  req3.resume();
  req3.end();
}));
