// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { createQuicSocket } = require('net');
const { once } = require('events');
const fs = require('fs');
const qlog = process.env.NODE_QLOG === '1';

const { key, cert, ca } = require('../common/quic');
const options = { key, cert, ca, alpn: 'meow', qlog };

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
  const server = createQuicSocket({ qlog, server: options });
  const client = createQuicSocket({ qlog, client: options });
  let fd;

  server.on('session', common.mustCall(async (session) => {
    if (qlog) {
      session.qlog.pipe(
        fs.createWriteStream(`server-${variant}-${offset}-${length}.qlog`));
    }

    const stream = await session.openStream({ halfOpen: true });

    // The data and end events won't emit because
    // the stream is never readable.
    stream.on('data', common.mustNotCall());
    stream.on('end', common.mustNotCall());
    stream.on('finish', common.mustCall());
    stream.on('close', common.mustCall());

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

    session.on('close', common.mustCall());
  }));

  await server.listen();

  const req = await client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port
  });
  if (qlog) {
    req.qlog.pipe(
      fs.createWriteStream(`client-${variant}-${offset}-${length}.qlog`));
  }

  req.on('stream', common.mustCall((stream) => {
    const data = [];
    stream.on('data', (chunk) => data.push(chunk));
    stream.on('end', common.mustCall(() => {
      let expectedContent = fs.readFileSync(__filename);
      if (offset !== -1) expectedContent = expectedContent.slice(offset);
      if (length !== -1) expectedContent = expectedContent.slice(0, length);
      assert.deepStrictEqual(Buffer.concat(data), expectedContent);
      if (fd !== undefined) {
        if (fd.close) fd.close().then(common.mustCall());
        else fs.closeSync(fd);
      }
    }));
    stream.on('close', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));

  await Promise.all([
    once(client, 'close'),
    once(server, 'close')
  ]);
}
