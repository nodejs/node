'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const net = require('net');

// Regression test: inbound DATA frames handed to JS on an undrained stream must
// count against maxSessionMemory. Before the fix a peer could send many tiny
// DATA frames on a stream the application never reads, and the session would
// keep reading (and buffering) them. Because each frame becomes a Buffer view in
// the readable queue, memory grew in proportion to the *number* of frames rather
// than the bytes sent, so a single stream could make the process retain hundreds
// of MB. With the fix the session charges the queued data against
// maxSessionMemory and stops reading once it can no longer afford to buffer
// more, keeping the readable buffer bounded.
const maxSessionMemory = 1;
const chunkSize = 1;
const totalFrames = 200000;

let peakReadableLength = 0;

function frame(type, flags, id, payload) {
  const header = Buffer.alloc(9);
  header.writeUIntBE(payload.length, 0, 3);
  header[3] = type;
  header[4] = flags;
  header.writeUInt32BE(id, 5);
  return Buffer.concat([header, payload]);
}

let session = null;
let probe = null;

const server = http2.createServer({ maxSessionMemory });
server.on('session', (s) => {
  session = s;
  s.on('error', () => {});
});
server.on('stream', (stream) => {
  stream.on('error', () => {});
  // The application never reads the request body, so the stream stays
  // undrained. Record the largest amount of data the readable buffer ever holds;
  // with the fix that stays bounded by maxSessionMemory instead of growing to
  // match the number of frames the peer sends.
  probe = setInterval(() => {
    peakReadableLength = Math.max(peakReadableLength, stream.readableLength);
  }, 10);
});

server.listen(0, common.mustCall(() => {
  const sock = net.connect(server.address().port, '127.0.0.1');
  sock.on('error', () => {});

  sock.on('connect', () => {
    sock.write(Buffer.from('PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n'));
    // Client SETTINGS.
    sock.write(frame(4, 0, 0, Buffer.alloc(0)));
  });

  let settingsAcked = false;
  sock.on('data', (chunk) => {
    if (!settingsAcked && chunk.length >= 9 && chunk[3] === 4) {
      settingsAcked = true;
      // ACK the server's SETTINGS.
      sock.write(Buffer.from([0, 0, 0, 4, 1, 0, 0, 0, 0]));
      // Open one stream (END_HEADERS) and spray 1-byte DATA frames at it.
      const hpack = Buffer.from([0x83, 0x86, 0x84, 0x01, 0x01, 0x61]);
      sock.write(frame(1, 4, 1, hpack));
      const payload = Buffer.alloc(chunkSize, 0x61);
      const parts = [];
      for (let i = 0; i < totalFrames; i++) {
        parts.push(frame(0, 0, 1, payload));
      }
      sock.write(Buffer.concat(parts));
    }
  });

  setTimeout(common.mustCall(() => {
    if (probe) clearInterval(probe);
    sock.destroy();
    if (session) session.destroy();
    server.close();
    // The session must not let one undrained stream buffer every frame it was
    // sent. Before the fix the readable buffer grew to totalFrames; with the fix
    // the backpressure keeps it bounded well below half of that.
    assert.ok(peakReadableLength < totalFrames / 2,
              `expected backpressure to bound readableLength, got ${peakReadableLength}`);
  }), 4000);
}));
