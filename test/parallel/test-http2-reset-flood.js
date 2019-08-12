'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const child_process = require('child_process');
const http2 = require('http2');
const net = require('net');

// Verify that creating a number of invalid HTTP/2 streams will eventually
// result in the peer closing the session.
// This test uses separate processes for client and server to avoid
// the two event loops intermixing, as we are writing in a busy loop here.

if (process.argv[2] === 'child') {
  const server = http2.createServer();
  server.on('stream', (stream) => {
    stream.respond({
      'content-type': 'text/plain',
      ':status': 200
    });
    stream.end('Hello, world!\n');
  });
  server.listen(0, () => {
    process.stdout.write(`${server.address().port}`);
  });
  return;
}

const child = child_process.spawn(process.execPath, [__filename, 'child'], {
  stdio: ['inherit', 'pipe', 'inherit']
});
child.stdout.on('data', common.mustCall((port) => {
  const h2header = Buffer.alloc(9);
  const conn = net.connect(+port);

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

  let i = 1;
  function writeRequests() {
    for (; !gotError; i += 2) {
      h2header[3] = 1;  // HEADERS
      h2header[4] = 0x5;  // END_HEADERS|END_STREAM
      h2header.writeIntBE(1, 0, 3);  // Length: 1
      h2header.writeIntBE(i, 5, 4);  // Stream ID
      // 0x88 = :status: 200
      conn.write(Buffer.concat([h2header, Buffer.from([0x88])]));

      if (i % 1000 === 1) {
        // Delay writing a bit so we get the chance to actually observe
        // an error. This is not necessary on master/v12.x, because there
        // conn.write() can fail directly when writing to a connection
        // that was closed by the remote peer due to
        // https://github.com/libuv/libuv/commit/ee24ce900e5714c950b248da2b
        i += 2;
        return setImmediate(writeRequests);
      }
    }
  }

  conn.once('error', common.mustCall(() => {
    gotError = true;
    child.kill();
    conn.destroy();
  }));
}));
