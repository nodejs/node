'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');
const net = require('net');

// When a peer sends GOAWAY(NO_ERROR), the session starts a graceful close
// on its own. If nghttp2 later terminates the session internally because of
// a protocol error (here, an oversized frame) while a stream is still open,
// the session must still be torn down and the error surfaced, rather than
// leaving the stream and connection hanging.

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  stream.on('error', common.expectsError({
    code: 'ERR_HTTP2_ERROR',
    name: 'Error',
    message: 'Protocol error'
  }));
  stream.on('close', common.mustCall());
}));

server.on('session', common.mustCall((session) => {
  session.on('goaway', common.mustCall());
  session.on('error', common.expectsError({
    code: 'ERR_HTTP2_ERROR',
    name: 'Error',
    message: 'Protocol error'
  }));

  session.on('close', common.mustCall(() => server.close()));
}));

server.listen(0, common.mustCall(() => {
  const conn = net.connect({
    port: server.address().port,
    allowHalfOpen: true,
  });

  // HTTP/2 client connection preface.
  conn.write('PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n');

  // Empty SETTINGS frame.
  const settingsFrame = Buffer.alloc(9);
  settingsFrame[3] = 0x04; // type: SETTINGS
  conn.write(settingsFrame);

  let inbuf = Buffer.alloc(0);
  let state = 'settingsHeader';
  let settingsFrameLength;

  conn.on('data', (chunk) => {
    inbuf = Buffer.concat([inbuf, chunk]);

    switch (state) {
      case 'settingsHeader':
        if (inbuf.length < 9) return;
        settingsFrameLength = inbuf.readUIntBE(0, 3);
        inbuf = inbuf.slice(9);
        state = 'readingSettings';
        // Fallthrough
      case 'readingSettings': {
        if (inbuf.length < settingsFrameLength) return;
        inbuf = inbuf.slice(settingsFrameLength);
        state = 'done';

        // ACK the server SETTINGS.
        const ack = Buffer.alloc(9);
        ack[3] = 0x04; // type: SETTINGS
        ack[4] = 0x01; // flag: ACK
        conn.write(ack);

        // Open stream 1 without END_STREAM, so it stays open on the server.
        // HPACK: :method GET (0x82), :scheme http (0x86), :path / (0x84),
        // :authority localhost (literal with indexed name 1).
        const block = Buffer.from([
          0x82, 0x86, 0x84, 0x41, 0x09,
          ...Buffer.from('localhost'),
        ]);
        const headers = Buffer.alloc(9);
        headers.writeUIntBE(block.length, 0, 3);
        headers[3] = 0x01; // type: HEADERS
        headers[4] = 0x04; // flags: END_HEADERS
        headers.writeUInt32BE(1, 5); // stream id: 1
        conn.write(Buffer.concat([headers, block]));

        // GOAWAY(NO_ERROR), last stream id 0. This makes the server
        // gracefully close its session without any application code.
        const goaway = Buffer.alloc(17);
        goaway.writeUIntBE(8, 0, 3); // length: 8
        goaway[3] = 0x07; // type: GOAWAY
        goaway.writeUInt32BE(0, 9); // last stream id
        goaway.writeUInt32BE(0, 13); // error code: NO_ERROR
        conn.write(goaway);

        // Send a DATA frame header claiming length 16385, which exceeds the
        // default max_frame_size of 16384, so nghttp2 calls
        // nghttp2_session_terminate_session(FRAME_SIZE_ERROR) internally.
        const oversized = Buffer.alloc(9);
        oversized.writeUIntBE(16385, 0, 3); // length: 16385 (one over max)
        oversized[3] = 0x00; // type: DATA
        oversized.writeUInt32BE(1, 5); // stream id: 1
        conn.write(oversized);
      }
    }
  });

  // The server must close the connection after sending GOAWAY:
  conn.on('end', common.mustCall(() => conn.end()));
  conn.on('close', common.mustCall());
}));
