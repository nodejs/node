'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

common.expectWarning(
  'DeprecationWarning',
  'http2Stream.priority is longer supported after priority signalling was deprecated in RFC 1993',
  'DEP0194');

const server = h2.createServer();

server.on('stream', common.mustCall((stream) => {
  assert(stream.session);
  stream.session.destroy();
  assert.strictEqual(stream.session, undefined);

  // Test that stream.state getter returns an empty object
  // when the stream session has been destroyed
  assert.deepStrictEqual({}, stream.state);

  // Test that ERR_HTTP2_INVALID_STREAM is thrown while calling
  // stream operations after the stream session has been destroyed
  const invalidStreamError = {
    name: 'Error',
    code: 'ERR_HTTP2_INVALID_STREAM',
    message: 'The stream has been destroyed'
  };
  assert.throws(() => stream.additionalHeaders(), invalidStreamError);
  assert.throws(() => stream.priority(), invalidStreamError);
  assert.throws(() => stream.respond(), invalidStreamError);
  assert.throws(
    () => stream.pushStream({}, common.mustNotCall()),
    {
      code: 'ERR_HTTP2_PUSH_DISABLED',
      name: 'Error'
    }
  );
  // When session is destroyed all streams are destroyed and no further
  // error should be emitted.
  stream.on('error', common.mustNotCall());
  assert.strictEqual(stream.write('data', common.expectsError({
    name: 'Error',
    code: 'ERR_STREAM_WRITE_AFTER_END',
    message: 'write after end'
  })), false);
}));

server.listen(0, common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();
  req.resume();
  req.on('end', common.mustCall());
  req.on('close', common.mustCall(() => server.close(common.mustCall())));
}));
