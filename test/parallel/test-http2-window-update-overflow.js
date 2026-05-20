'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');
const net = require('net');

// Regression test: a connection-level WINDOW_UPDATE that causes the flow
// control window to exceed 2^31-1 must destroy the Http2Session (not leak it).
//
// nghttp2 responds with GOAWAY(FLOW_CONTROL_ERROR) internally but previously
// Node's OnInvalidFrame callback only propagated errors for
// NGHTTP2_ERR_STREAM_CLOSED and NGHTTP2_ERR_PROTO. The missing
// NGHTTP2_ERR_FLOW_CONTROL case left the session unreachable after the GOAWAY,
// causing a memory leak.

const server = http2.createServer();

server.on('session', common.mustCall((session) => {
  session.on('error', common.mustCall());
  session.on('close', common.mustCall(() => server.close()));
}));

server.listen(0, common.mustCall(() => {
  const conn = net.connect({
    port: server.address().port,
    allowHalfOpen: true,
  });

  // HTTP/2 client connection preface.
  conn.write('PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n');

  // Empty SETTINGS frame (9-byte header, 0-byte payload).
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

        // WINDOW_UPDATE on stream 0 (connection level) with increment 2^31-1.
        // Default connection window is 65535, so the new total would be
        // 65535 + 2147483647 = 2147549182 > 2^31-1, triggering
        // NGHTTP2_ERR_FLOW_CONTROL inside nghttp2.
        const windowUpdate = Buffer.alloc(13);
        windowUpdate.writeUIntBE(4, 0, 3);          // length = 4
        windowUpdate[3] = 0x08;                      // type: WINDOW_UPDATE
        windowUpdate[4] = 0x00;                      // flags: none
        windowUpdate.writeUIntBE(0, 5, 4);           // stream id: 0
        windowUpdate.writeUIntBE(0x7FFFFFFF, 9, 4);  // increment: 2^31-1
        conn.write(windowUpdate);
      }
    }
  });

  // The server must close the connection after sending GOAWAY.
  conn.on('end', common.mustCall(() => conn.end()));
  conn.on('close', common.mustCall());
}));
