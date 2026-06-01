'use strict';

// Server-side auto-drain (mirrors HTTP/1's req._dump): when a server
// finishes its response without ever consuming the request body, the
// body is silently drained on 'finish' so 'end' can fire and the
// stream closes cleanly - no 'aborted', no 'error'.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();

server.on('stream', common.mustCall((stream, headers) => {
  // Deliberately do NOT read the request body. Just respond and end.
  // The server-side auto-drain should kick in on 'finish' and pull
  // the body off the wire so 'end' can fire.
  stream.respond({ ':status': 200 });
  stream.end('ok');

  // 'end' on the readable must fire - the auto-drain doing its job.
  stream.on('end', common.mustCall(() => {
    assert.strictEqual(stream._readableState.endEmitted, true);
  }));

  stream.on('aborted', common.mustNotCall(
    "'aborted' must not fire when the auto-drain succeeded"));
  stream.on('error', common.mustNotCall(
    "'error' must not fire on a clean response without reading the body"));

  stream.on('close', common.mustCall(() => {
    // stream.aborted must be false after auto-drain.
    assert.strictEqual(stream.aborted, false);
    assert.strictEqual(stream.destroyed, true);
    server.close();
  }));
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  // Send a non-trivial request body so there's actual data buffered on
  // the server side before the auto-drain runs. Without the drain,
  // `endEmitted` would stay false until the buffer is consumed.
  const cs = client.request({ ':method': 'POST' });
  cs.resume();
  cs.on('close', common.mustCall(() => client.close()));
  cs.end(Buffer.alloc(16 * 1024));
}));
