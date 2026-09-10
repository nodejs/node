'use strict';

// Regression test against deadlocks between two HTTP/2 peers that are both
// writing at the same time.
//
// To bound how much it buffered while output was backed up, an Http2Session
// used to stop reading from its socket whenever a write was in flight, and
// resume only once that write completed. When the peer was itself blocked
// writing, that write never completed, so the session never read again and
// the connection hung forever with no error and no timeout.
//
// Rather than relying on kernel socket buffers filling up - which depends on
// the platform and configured window sizes - this models one half of that
// cycle directly. The client's socket forwards a write but does not report it
// as complete, substituting for a write blocked because the peer is not
// reading. Only after that write is stalled does the server send its response
// body. A session that stops reading while writing never sees it.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const net = require('net');
const { Duplex } = require('stream');

const BODY = 'the response body';

const heldCallbacks = [];
let serverStream;

let stallWrites = false;

// Client-side socket that forwards writes to a real connection, but can leave
// their completion callbacks pending to model a transport-blocked write.
class StalledClientSocket extends Duplex {
  constructor(port) {
    super();
    this.inner = net.connect(port, common.localhostIPv4);
    this.inner.on('data', (chunk) => this.push(chunk));
  }
  _read() {
    // Incoming data is pushed as it arrives.
  }
  _write(chunk, encoding, callback) {
    this.inner.write(chunk, encoding);
    if (stallWrites) {
      heldCallbacks.push(callback);
      // Avoid writing from the server re-entrantly inside _write(). The
      // ordering is still explicit: this callback is already held.
      setImmediate(() => serverStream.end(BODY));
      return;
    }
    callback();
  }
  _final(callback) {
    callback();
  }
  _destroy(err, callback) {
    this.inner.destroy();
    callback(err);
  }
}

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  // Send headers first. Their response event starts the stalled client write.
  stream.respond();
  serverStream = stream;
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

  const client = http2.connect(`http://${common.localhostIPv4}:${port}`, {
    createConnection: () => new StalledClientSocket(port),
  });

  const req = client.request({ ':method': 'POST' });

  let received = '';

  req.on('response', common.mustCall(() => {
    // _write() will schedule the response body only after it has retained the
    // callback, guaranteeing that the native write is still in progress.
    stallWrites = true;
    req.write(Buffer.alloc(256));
  }));

  req.on('data', (chunk) => {
    received += chunk;
  });

  req.on('end', common.mustCall(() => {
    assert.strictEqual(received, BODY);
    assert.ok(heldCallbacks.length > 0,
              'test did not actually stall a socket write');

    // Let the stalled writes complete so that everything can shut down.
    stallWrites = false;
    for (const callback of heldCallbacks) callback();

    client.destroy();
    server.close();
  }));
}));
