// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { createQuicSocket } = require('net');
const fs = require('fs');

const { key, cert, ca } = require('../common/quic');

const variants = [];
for (const variant of ['sendFD', 'sendFile', 'sendFD+fileHandle']) {
  for (const offset of [-1, 0, 100]) {
    for (const length of [-1, 100]) {
      variants.push({ variant, offset, length });
    }
  }
}

for (const { variant, offset, length } of variants) {
  const server = createQuicSocket();
  let fd;

  server.listen({ key, cert, ca, alpn: 'meow' });

  server.on('session', common.mustCall((session) => {
    session.on('secure', common.mustCall((servername, alpn, cipher) => {
      const stream = session.openStream({ halfOpen: false });

      stream.on('data', common.mustNotCall());
      stream.on('finish', common.mustCall());
      stream.on('close', common.mustCall());
      stream.on('end', common.mustCall());

      if (variant === 'sendFD') {
        fd = fs.openSync(__filename, 'r');
        stream.sendFD(fd, { offset, length });
      } else if (variant === 'sendFD+fileHandle') {
        fs.promises.open(__filename, 'r').then(common.mustCall((handle) => {
          fd = handle;
          stream.sendFD(handle, { offset, length });
        }));
      } else {
        assert.strictEqual(variant, 'sendFile');
        stream.sendFile(__filename, { offset, length });
      }
    }));

    session.on('close', common.mustCall());
  }));

  server.on('ready', common.mustCall(() => {
    const client = createQuicSocket({
      client: { key, cert, ca, alpn: 'meow' } });

    const req = client.connect({
      address: 'localhost',
      port: server.endpoints[0].address.port
    });

    req.on('stream', common.mustCall((stream) => {
      const data = [];
      stream.on('data', (chunk) => data.push(chunk));
      stream.on('end', common.mustCall(() => {
        let expectedContent = fs.readFileSync(__filename);
        if (offset !== -1) expectedContent = expectedContent.slice(offset);
        if (length !== -1) expectedContent = expectedContent.slice(0, length);
        assert.deepStrictEqual(Buffer.concat(data), expectedContent);

        stream.end();
        client.close();
        server.close();
        if (fd !== undefined) {
          if (fd.close) fd.close().then(common.mustCall());
          else fs.closeSync(fd);
        }
      }));
    }));

    req.on('close', common.mustCall());
  }));

  server.on('close', common.mustCall());
}
