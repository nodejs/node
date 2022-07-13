'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const net = require('net');

// Verify that creating a number of invalid HTTP/2 streams will
// result in the peer closing the session within maxSessionInvalidFrames
// frames.

const maxSessionInvalidFrames = 100;
const server = http2.createServer({ maxSessionInvalidFrames });
server.on('stream', (stream) => {
  stream.respond({
    'content-type': 'text/plain',
    ':status': 200
  });
  stream.end('Hello, world!\n');
});

server.listen(0, () => {
  const h2header = Buffer.alloc(9);
  const conn = net.connect({
    port: server.address().port,
    allowHalfOpen: true
  });

  conn.write('PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n');

  h2header[3] = 4;  // Send a settings frame.
  conn.write(Buffer.from(h2header));

  let inbuf = Buffer.alloc(0);
  let state = 'settingsHeader';
  let settingsFrameLength;
  conn.on('data', (chunk) => {
    inbuf = Buffer.concat([inbuf, chunk]);
    switch (state) {
      case 'settingsHeader':
        if (inbuf.length < 9) return;
        settingsFrameLength = inbuf.readIntBE(0, 3);
        inbuf = inbuf.slice(9);
        state = 'readingSettings';
        // Fallthrough
      case 'readingSettings':
        if (inbuf.length < settingsFrameLength) return;
        inbuf = inbuf.slice(settingsFrameLength);
        h2header[3] = 4;  // Send a settings ACK.
        h2header[4] = 1;
        conn.write(Buffer.from(h2header));
        state = 'ignoreInput';
        writeRequests();
    }
  });

  let gotError = false;
  let streamId = 1;
  let reqCount = 0;

  function writeRequests() {
    for (let i = 1; i < 10 && !gotError; i++) {
      h2header[3] = 1;  // HEADERS
      h2header[4] = 0x5;  // END_HEADERS|END_STREAM
      h2header.writeIntBE(1, 0, 3);  // Length: 1
      h2header.writeIntBE(streamId, 5, 4);  // Stream ID
      streamId += 2;
      // 0x88 = :status: 200
      if (!conn.write(Buffer.concat([h2header, Buffer.from([0x88])]))) {
        break;
      }
      reqCount++;
    }
    // Timeout requests to slow down the rate so we get more accurate reqCount.
    if (!gotError)
      setTimeout(writeRequests, 10);
  }

  conn.once('error', common.mustCall(() => {
    gotError = true;
    assert.ok(Math.abs(reqCount - maxSessionInvalidFrames) < 100,
              `Request count (${reqCount}) must be around (±100)` +
      ` maxSessionInvalidFrames option (${maxSessionInvalidFrames})`);
    conn.destroy();
    server.close();
  }));
});
