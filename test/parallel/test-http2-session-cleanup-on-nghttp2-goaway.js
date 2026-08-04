'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');
const net = require('net');

// When nghttp2 internally sends a GOAWAY frame due to a protocol error, it
// may call nghttp2_session_terminate_session() directly, bypassing the
// on_invalid_frame_recv_callback entirely. This test ensures that even
// in that scenario, we still correctly clean up the session & connection.
//
// This test reproduces this with a client who sends a frame header with
// a length exceeding the default max_frame_size (16384). nghttp2 responds
// with GOAWAY(FRAME_SIZE_ERROR) without notifying Node through any callback.

const server = http2.createServer();

server.on('session', common.mustCall((session) => {
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

        // Send a HEADERS frame header claiming length 16385, which exceeds
        // the default max_frame_size of 16384. nghttp2 checks the length
        // before reading any payload, so no body is needed. This triggers
        // nghttp2_session_terminate_session(FRAME_SIZE_ERROR) directly in
        // nghttp2_session_mem_recv2 — bypassing on_invalid_frame_recv_callback.
        const oversized = Buffer.alloc(9);
        oversized.writeUIntBE(16385, 0, 3); // length: 16385 (one over max)
        oversized[3] = 0x01;                // type: HEADERS
        oversized[4] = 0x04;                // flags: END_HEADERS
        oversized.writeUInt32BE(1, 5);       // stream id: 1
        conn.write(oversized);

        // No need to write the data - the header alone triggers the check.
      }
    }
  });

  // The server must close the connection after sending GOAWAY:
  conn.on('end', common.mustCall(() => conn.end()));
  conn.on('close', common.mustCall());
}));
