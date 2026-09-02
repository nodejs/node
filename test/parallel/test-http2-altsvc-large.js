'use strict';

// Regression test for a one-byte out-of-bounds write in Http2Session::AltSvc.
// The native handler allocated origin/value buffers sized to the string length
// but wrote them with a null terminator, so an origin or alt value longer than
// the inline stack buffer (1024 bytes) overflowed the heap allocation by one
// byte. Exercise both paths with values above that threshold and confirm the
// frames round-trip intact.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const Countdown = require('../common/countdown');

// alt is limited to a quoted-string; padding is well past the 1024-byte inline
// buffer so the value is heap-allocated at its exact length.
const largeAlt = `h2=":8000"; ma=${'0'.repeat(2000)}`;
const largeOrigin = `https://${'a'.repeat(1200)}.example.org`;

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  // origin is empty here, so this exercises the value (alt) buffer.
  stream.session.altsvc(largeAlt, stream.id);
  stream.respond();
  stream.end('ok');
}));
server.on('session', common.mustCall((session) => {
  // stream id 0 with a long origin exercises the origin buffer.
  session.altsvc('h2=":8000"', largeOrigin);
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const countdown = new Countdown(2, () => {
    client.close();
    server.close();
  });

  client.on('altsvc', common.mustCall((alt, origin, stream) => {
    if (stream === 0) {
      assert.strictEqual(alt, 'h2=":8000"');
      assert.strictEqual(origin, new URL(largeOrigin).origin);
    } else {
      assert.strictEqual(alt, largeAlt);
      assert.strictEqual(origin, '');
    }
    countdown.dec();
  }, 2));

  const req = client.request();
  req.resume();
  req.on('close', common.mustCall());
}));
