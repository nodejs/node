// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { createQuicSocket } = require('net');
const { once } = require('events');
const fs = require('fs');

const { key, cert, ca } = require('../common/quic');
const options = { key, cert, ca, alpn: 'meow' };

const variants = [];
for (const variant of ['sendFD', 'sendFile', 'sendFD+fileHandle']) {
  for (const offset of [-1, 0, 100]) {
    for (const length of [-1, 100]) {
      variants.push({ variant, offset, length });
    }
  }
}

(async function() {
  await Promise.all(variants.map(test));
})().then(common.mustCall());

async function test({ variant, offset, length }) {
  const server = createQuicSocket({ server: options });
  const client = createQuicSocket({ client: options });
  let fd;

  server.on('session', common.mustCall((session) => {
    session.on('secure', common.mustCall((servername, alpn, cipher) => {
      const stream = session.openStream({ halfOpen: true });

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

  await server.listen();

  const req = await client.connect({
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

      client.close();
      server.close();
      if (fd !== undefined) {
        if (fd.close) fd.close().then(common.mustCall());
        else fs.closeSync(fd);
      }
    }));
  }));

  await Promise.all([
    once(client, 'close'),
    once(server, 'close')
  ]);
}
