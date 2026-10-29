// Flags: --expose-gc
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// A stream that is reset because of an oversized header block keeps the
// headers it had already accepted. If the stream is destroyed while one of
// its writes is still on the socket, it outlives the session, and the session
// destructor used to abort because that memory was still allocated.

const http2 = require('http2');
const net = require('net');
const { setTimeout: sleep } = require('timers/promises');

// Enough response data to fill the socket buffers on loopback.
const kStreams = 80;

function frame(type, flags, id, payload = Buffer.alloc(0)) {
  const header = Buffer.alloc(9);
  header.writeUIntBE(payload.length, 0, 3);
  header[3] = type;
  header[4] = flags;
  header.writeUInt32BE(id, 5);
  return Buffer.concat([header, payload]);
}

// :method POST, :scheme http, :path /, :authority localhost
const requestHeaders = Buffer.from([
  0x83, 0x86, 0x84, 0x01, 0x09, ...Buffer.from('localhost'),
]);

// More trailer fields than the default maxHeaderListPairs (128).
const trailers = Buffer.concat(Array.from({ length: 200 }, (_, i) => {
  const name = `x-${i}`;
  return Buffer.from([0x00, name.length, ...Buffer.from(name), 0x01, 0x76]);
}));

const server = http2.createServer(common.mustCall((req, res) => {
  req.on('error', () => {});
  res.on('error', () => {});
  // Respond before the request has ended, with more data than the client
  // reads, so that the response stays in a pending socket write.
  res.end(Buffer.alloc(100 * 1024));
}, kStreams));

server.on('session', common.mustCall((session) => {
  session.on('error', () => {});
  session.on('close', common.mustCall(async () => {
    server.close();
    // Collect the session before the streams that are still referenced by
    // their pending writes.
    for (let i = 0; i < 10; i++) {
      globalThis.gc();
      await sleep(10);
    }
  }));
}));

server.listen(0, common.mustCall(async () => {
  const client = net.connect(server.address().port);
  client.on('error', () => {});
  client.pause();

  const settings = Buffer.from([0x00, 0x04, 0x7f, 0xff, 0xff, 0xff]);
  const windowUpdate = Buffer.alloc(4);
  windowUpdate.writeUInt32BE(0x7fffffff - 65535);
  const requests = [];
  for (let i = 0; i < kStreams; i++)
    requests.push(frame(1, 0x4, 1 + 2 * i, requestHeaders));
  client.write(Buffer.concat([
    Buffer.from('PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n'),
    frame(4, 0, 0, settings),
    frame(8, 0, 0, windowUpdate),
    ...requests,
  ]));
  await sleep(common.platformTimeout(500));

  const blocks = [];
  for (let i = 0; i < kStreams; i++)
    blocks.push(frame(1, 0x5, 1 + 2 * i, trailers));
  client.write(Buffer.concat(blocks));
  await sleep(common.platformTimeout(200));

  client.resetAndDestroy();
}));
