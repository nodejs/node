'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');

// Regression test for https://github.com/nodejs/node/issues/64850
//
// Destroying the session from a 'stream' handler runs (via nextTick drained
// from MakeCallback) while nghttp2 is still inside mem_recv. Close is deferred
// for that window; later HEADERS/DATA in the same buffer must not abort with
// Assertion failed: onread->IsFunction().

const STREAMS = 8;
const BODY = Buffer.alloc(2048, 'a');
const ROUNDS = 40;

const server = http2.createServer({
  settings: { maxConcurrentStreams: 4 },
});

server.on('session', (session) => session.on('error', () => {}));

server.on('stream', (stream) => {
  stream.on('error', () => {});
  stream.session.destroy();
});

server.listen(0, '127.0.0.1', common.mustCall(() => {
  const port = server.address().port;
  const origin = `http://127.0.0.1:${port}`;
  let remaining = ROUNDS;

  const round = () => {
    if (remaining-- <= 0) {
      server.close();
      return;
    }

    const session = http2.connect(origin);
    session.on('error', () => {});
    session.on('close', () => setImmediate(round));

    session.on('connect', () => {
      for (let i = 0; i < STREAMS; i++) {
        const stream = session.request({
          ':path': `/${i}`,
          ':method': 'POST',
        });
        stream.on('error', () => {});
        stream.resume();
        stream.end(BODY);
      }
    });
  };

  round();
}));
