'use strict';

// HTTP/1 parity for fire-and-forget clients: when no 'response'
// listener is attached at the time response headers arrive, the
// response body is silently drained (mirrors res._dump from
// parserOnIncomingClient). After that drains, 'end' fires and the
// stream closes cleanly — no 'aborted', no 'error'.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  // Send a non-trivial response body so the client's readable buffer
  // would actually contain something to discard.
  stream.respond({ ':status': 200 });
  stream.end(Buffer.alloc(16 * 1024));
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const cs = client.request({ ':method': 'GET' });

  // Deliberately do NOT attach a 'response' listener — that's the
  // condition that triggers H1's `_dump` and our parallel auto-discard.
  // Adding a listener here (even a `common.mustNotCall`) would make
  // listenerCount > 0 and skip the auto-discard.

  cs.on('aborted', common.mustNotCall(
    "'aborted' must not fire for fire-and-forget clients"));
  cs.on('error', common.mustNotCall(
    "'error' must not fire for fire-and-forget clients"));

  // 'end' must fire once auto-discard resumes the readable.
  cs.on('end', common.mustCall());

  cs.on('close', common.mustCall(() => {
    assert.strictEqual(cs.aborted, false);
    assert.strictEqual(cs.destroyed, true);
    client.close();
    server.close();
  }));

  cs.end();
}));
