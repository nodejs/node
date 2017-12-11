'use strict';
// Flags: --expose-internals

// Tests that calling unref() on Http2Session:
// (1) Prevents it from keeping the process alive
// (2) Doesn't crash

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const makeDuplexPair = require('../common/duplexpair');

const server = http2.createServer();
const { clientSide, serverSide } = makeDuplexPair();

// 'session' event should be emitted 3 times:
// - the vanilla client
// - the destroyed client
// - manual 'connection' event emission with generic Duplex stream
server.on('session', common.mustCallAtLeast((session) => {
  session.unref();
}, 3));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

  // unref new client
  {
    const client = http2.connect(`http://localhost:${port}`);
    client.unref();
  }

  // unref destroyed client
  {
    const client = http2.connect(`http://localhost:${port}`);
    client.destroy();
    client.unref();
  }

  // unref destroyed client
  {
    const client = http2.connect(`http://localhost:${port}`, {
      createConnection: common.mustCall(() => clientSide)
    });
    client.destroy();
    client.unref();
  }
}));
server.emit('connection', serverSide);
server.unref();

setTimeout(common.mustNotCall(() => {}), 1000).unref();
