'use strict';

// CVE-2019-9517 describes a peer that advertises HTTP/2 flow-control credit
// but does not drain its TCP socket, leaving the server holding responses it
// cannot write out.
//
// Node bounds this with maxSessionMemory. Once the budget is spent, further
// streams are refused rather than queued, so a client cannot make the server
// buffer a response for every request it opens.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const net = require('net');
const { Duplex } = require('stream');

const TOTAL_REQUESTS = 200;
// Accepted count depends on kernel buffer sizes but this seems to
// cover most likely scenarios:
const MAX_ACCEPTED = 20;
const body = Buffer.alloc(1_000_000);

let accepted = 0;
let client;

// Client-side socket that forwards writes but never consumes incoming data.
class UnreadClientSocket extends Duplex {
  constructor(port) {
    super();
    this.inner = net.connect(port, common.localhostIPv4);
    this.inner.pause();
  }
  _read() {
    // Deliberately never pull from the underlying socket.
  }
  _write(chunk, encoding, callback) {
    this.inner.write(chunk, encoding, callback);
  }
  _final(callback) {
    callback();
  }
  _destroy(err, callback) {
    this.inner.destroy();
    callback(err);
  }
}

const onStreamsRefused = common.mustCall(() => {
  assert.ok(accepted >= 1, 'client never reached the server');
  client.destroy();
  server.close();
});

const server = http2.createServer({
  maxSessionMemory: 1,
  // Turn the rejected streams into an observable server-session failure.
  maxSessionRejectedStreams: 0,
});

server.on('session', common.mustCall((session) => {
  session.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_HTTP2_ERROR');
    onStreamsRefused();
  }));
}));

// At least one stream must be accepted, otherwise the assertion above could
// pass without the client ever having reached the server.
server.on('stream', common.mustCallAtLeast((stream) => {
  accepted++;
  // Assert here rather than once the session fails: if the bound does not
  // hold there may be no session failure at all.
  assert.ok(accepted <= MAX_ACCEPTED,
            `server accepted ${accepted} of ${TOTAL_REQUESTS} streams ` +
            'from a peer that never reads; maxSessionMemory did not ' +
            'bound its outbound buffering');
  // The fatal session error also destroys every accepted stream.
  stream.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_HTTP2_ERROR');
  }));
  stream.respond();
  stream.end(body);
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

  client = http2.connect(`http://${common.localhostIPv4}:${port}`, {
    createConnection: () => new UnreadClientSocket(port),
  });

  client.on('connect', common.mustCall(() => {
    for (let i = 0; i < TOTAL_REQUESTS; i++) {
      const req = client.request();
      req.end();
    }
  }));
}));
