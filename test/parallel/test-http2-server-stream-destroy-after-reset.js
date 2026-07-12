'use strict';

// Regression test for the zombie-stream invariant: a server-side
// Http2Stream that receives a peer RST_STREAM MUST eventually be
// destroyed, even when pending `_write` callbacks never fire. This
// surfaced as test-http2-close-while-writing.js flaking on macOS
// (~1 in ~6,700 CI runs); the underlying state is platform-independent
// and produced deterministically here.
//
// Only asserts the invariant ('close' fires within a bounded time).
// The full abort-behavior contract is covered by
// test-http2-reset-aborts-readable-not-drained.js and
// test-http2-reset-happy-close.js.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const http2 = require('http2');

const key = fixtures.readKey('agent8-key.pem', 'binary');
const cert = fixtures.readKey('agent8-cert.pem', 'binary');
const ca = fixtures.readKey('fake-startcom-root-cert.pem', 'binary');

const server = http2.createSecureServer({
  key,
  cert,
  // Constrains buffering so the 64 KB body arrives in several DATA frames,
  // making the server's 'data' listener fire repeatedly and queue several
  // 1-byte writes.
  maxSessionMemory: 1000,
});

let client;
let clientStream;
let serverStream;

// If the bug is present the server stream stays a zombie and `'close'`
// never fires, hanging the test until the runner times out. Fail fast
// with a useful diagnostic instead.
const failTimer = setTimeout(() => {
  assert.fail(
    'server stream is still a zombie after the peer sent RST_STREAM. ' +
    `destroyed=${serverStream?.destroyed} ` +
    `closed=${serverStream?.closed} ` +
    `aborted=${serverStream?.aborted} ` +
    `writableFinished=${serverStream?.writableFinished} ` +
    `writableLength=${serverStream?.writableLength}`);
}, common.platformTimeout(5000));

server.on('session', common.mustCall((session) => {
  session.on('stream', common.mustCall((stream) => {
    serverStream = stream;
    stream.resume();
    stream.on('data', function() {
      // Multiple invocations ⇒ multiple writes ⇒ stranded `_write`
      // callbacks once the peer RSTs the stream before they complete.
      this.write(Buffer.alloc(1));
      process.nextTick(() => clientStream.destroy());
    });

    // This regression test only cares that 'close' fires; swallow any
    // 'error' the abort path emits.
    stream.on('error', () => {});

    // The actual assertion: the server-side stream must reach 'close'
    // (i.e. `_destroy` must run) even though some of its `_write`
    // callbacks will never fire.
    stream.on('close', common.mustCall(() => {
      clearTimeout(failTimer);
      assert.strictEqual(stream.destroyed, true);
      client.close();
      server.close();
    }));
  }));
}));

server.listen(0, common.mustCall(() => {
  client = http2.connect(`https://localhost:${server.address().port}`, {
    ca,
    maxSessionMemory: 1000,
  });

  clientStream = client.request({ ':method': 'POST' });
  clientStream.resume();
  clientStream.write(Buffer.alloc(64 * 1024));

  // Deliberately do NOT call client.close()/server.close() here. Doing so
  // triggers the graceful-close drain in src/node_http2.cc which can
  // cascade-destroy the server-side zombie as a side-effect, masking the
  // bug on platforms where that drain fires reliably. Cleanup happens
  // from the server stream's 'close' handler above instead.
  clientStream.on('close', common.mustCall());
}));
