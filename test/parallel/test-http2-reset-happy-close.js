'use strict';

// A peer RST_STREAM that arrives AFTER our readable has fully drained
// AND while writes are still in flight is treated as a clean cancel:
// no 'error'. 'finish' must not fire (writes were aborted), but every
// pending _write callback eventually fires (success for writes that
// made it onto the wire, ECANCELED for those still queued at reset
// time). 'aborted' fires per the legacy criterion since the writable
// was still open at close - see DEP0207.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const { NGHTTP2_NO_ERROR } = http2.constants;

const N_WRITES = 16;
const CHUNK_BYTES = 8192;
// 16 * 8 KiB = 128 KiB, larger than the default 64 KiB initial flow-
// control window - so even if the OS socket buffer accepts everything
// instantly, the server's nghttp2 will hold some chunks in its outbound
// queue (waiting on a WINDOW_UPDATE that the client will never send).
// That guarantees we exercise the stranded-writes path even when the
// client RSTs after consuming HEADERS.

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  // Drain the request body fully.
  stream.resume();
  stream.on('end', common.mustCall(() => {
    // Readable is drained. Now begin streaming a response that is too
    // large to land before the client RSTs.
    stream.respond({ ':status': 200 });

    let writeCbCount = 0;
    for (let i = 0; i < N_WRITES; i++) {
      stream.write(Buffer.alloc(CHUNK_BYTES), () => { writeCbCount++; });
    }

    stream.on('aborted', common.mustCall(() => {
      assert.strictEqual(stream.aborted, true);
    }));
    stream.on('error', common.mustNotCall(
      "'error' must not fire on happy close"));
    stream.on('finish', common.mustNotCall(
      "'finish' must not fire when writable was aborted by reset"));

    stream.on('close', common.mustCall(() => {
      // 'aborted' fires per the legacy criterion (writable was still
      // open at reset).
      assert.strictEqual(stream.aborted, true);
      // 'finish' must not have been emitted (writes were aborted).
      assert.strictEqual(stream.writableFinished, false);
      assert.strictEqual(stream.destroyed, true);
      // The C++ Http2Stream::Destroy SetImmediate flushes any
      // still-queued WriteWraps with UV_ECANCELED, which fires the
      // remaining `_write` callbacks. Those land *after* 'close';
      // verify the full set has fired by the next two immediates.
      setImmediate(common.mustCall(() => setImmediate(common.mustCall(() => {
        assert.strictEqual(writeCbCount, N_WRITES,
                           `all ${N_WRITES} write callbacks must fire ` +
                           `(got ${writeCbCount})`);
        server.close();
      }))));
    }));
  }));
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const cs = client.request({ ':method': 'POST' });
  cs.on('close', common.mustCall(() => client.close()));

  // Tiny request body; end immediately so the server's readable drains
  // cleanly before we reset.
  cs.end(Buffer.alloc(8));

  cs.on('response', common.mustCall(() => {
    // We've seen the response HEADERS; the response body may or may not
    // have started arriving. RST without consuming any of it.
    cs.close(NGHTTP2_NO_ERROR);
  }));
}));
