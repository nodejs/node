'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

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
    type: Error,
    code: 'ERR_HTTP2_INVALID_STREAM',
    message: 'The stream has been destroyed'
  };
  common.expectsError(() => stream.additionalHeaders(), invalidStreamError);
  common.expectsError(() => stream.priority(), invalidStreamError);
  common.expectsError(() => stream.respond(), invalidStreamError);
  common.expectsError(
    () => stream.pushStream({}, common.mustNotCall()),
    {
      code: 'ERR_HTTP2_PUSH_DISABLED',
      type: Error
    }
  );
  stream.on('error', common.expectsError({
    type: Error,
    code: 'ERR_STREAM_WRITE_AFTER_END',
    message: 'write after end'
  }));
  assert.strictEqual(stream.write('data'), false);
}));

server.listen(0, common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();
  req.resume();
  req.on('end', common.mustCall());
  req.on('close', common.mustCall(() => server.close(common.mustCall())));
}));
