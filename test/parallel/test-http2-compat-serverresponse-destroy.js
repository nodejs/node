'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const Countdown = require('../common/countdown');

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
    process.nextTick(() => {
      assert.doesNotThrow(() => res.destroy(nextError));
    });
  }));

  if (req.url !== '/') {
    nextError = errors.shift();
  }

  res.destroy(nextError);
}, 3));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const countdown = new Countdown(3, () => {
    server.close();
    client.close();
  });

  {
    const req = client.request();
    req.on('response', common.mustNotCall());
    req.on('error', common.mustNotCall());
    req.on('end', common.mustCall());
    req.on('close', common.mustCall(() => countdown.dec()));
    req.resume();
  }

  {
    const req = client.request({ ':path': '/error' });

    req.on('response', common.mustNotCall());
    req.on('error', common.expectsError({
      code: 'ERR_HTTP2_STREAM_ERROR',
      type: Error,
      message: 'Stream closed with error code 2'
    }));
    req.on('close', common.mustCall(() => countdown.dec()));

    req.resume();
    req.on('end', common.mustCall());
  }

  {
    const req = client.request({ ':path': '/error' });

    req.on('response', common.mustNotCall());
    req.on('error', common.expectsError({
      code: 'ERR_HTTP2_STREAM_ERROR',
      type: Error,
      message: 'Stream closed with error code 2'
    }));
    req.on('close', common.mustCall(() => countdown.dec()));

    req.resume();
    req.on('end', common.mustCall());
  }
}));
