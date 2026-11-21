'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const net = require('net');

// A DATA chunk queued in a paused request body must not keep alive the
// whole socket read it arrived in. Each batch below is an ignored extension
// frame followed by a one-byte DATA frame.

const batchCount = 64;

function frame(type, flags, id, payload) {
  const header = Buffer.alloc(9);
  header.writeUIntBE(payload.length, 0, 3);
  header[3] = type;
  header[4] = flags;
  header.writeUInt32BE(id, 5);
  return Buffer.concat([header, payload]);
}

const filler = frame(0xb, 0, 0, Buffer.alloc(16_365, 0x5a));
const data = frame(0, 0, 1, Buffer.from('a'));
const batch = Buffer.concat([filler, data]);
const pingPayload = Buffer.from('datachnk');

let requestStream;

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  requestStream = stream;
  // Leave the request body unconsumed so the chunks stay queued.
}));

server.listen(0, common.mustCall(() => {
  const peer = net.connect(server.address().port);
  let received = Buffer.alloc(0);
  let settingsSeen = false;

  peer.on('connect', () => {
    peer.write(Buffer.from('PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n'));
    peer.write(frame(4, 0, 0, Buffer.alloc(0)));
  });

  peer.on('data', (chunk) => {
    received = Buffer.concat([received, chunk]);
    while (received.length >= 9) {
      const length = received.readUIntBE(0, 3);
      if (received.length < 9 + length) break;
      const type = received[3];
      const flags = received[4];
      const payload = received.subarray(9, 9 + length);
      received = received.subarray(9 + length);

      if (type === 4 && !(flags & 1) && !settingsSeen) {
        settingsSeen = true;
        peer.write(frame(4, 1, 0, Buffer.alloc(0)));
        const hpack = Buffer.from([0x83, 0x86, 0x84, 0x01, 0x01, 0x61]);
        peer.write(frame(1, 4, 1, hpack));
        for (let i = 0; i < batchCount; i++)
          peer.write(batch);
        // The PING is answered only after everything before it was parsed.
        peer.write(frame(6, 0, 0, pingPayload));
      } else if (type === 6 && (flags & 1) && payload.equals(pingPayload)) {
        check();
        peer.destroy();
        server.close();
      }
    }
  });
}));

function check() {
  const buffers = [];
  let buffer;
  while ((buffer = requestStream.read(1)) !== null)
    buffers.push(buffer);

  assert.strictEqual(buffers.length, batchCount);
  for (const chunk of buffers) {
    assert.strictEqual(chunk.toString(), 'a');
    assert.ok(chunk.buffer.byteLength < 1024,
              `a ${chunk.length} byte chunk retains ` +
              `${chunk.buffer.byteLength} bytes`);
  }
}
