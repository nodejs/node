// Flags: --no-warnings
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const { key, cert, ca } = require('../common/quic');

const { createWriteStream } = require('fs');
const { createQuicSocket } = require('net');
const { strictEqual } = require('assert');

const qlog = process.env.NODE_QLOG === '1';

const options = { key, cert, ca, alpn: 'zzz', qlog };

const client = createQuicSocket({ qlog, client: options });
const server = createQuicSocket({ qlog, server: options });

(async function() {
  server.on('session', common.mustCall(async (session) => {
    if (qlog) session.qlog.pipe(createWriteStream('server.qlog'));
    const stream = await session.openStream();
    stream.resume();
    stream.write('from the ');
    setTimeout(() => stream.end('server'), common.platformTimeout(10));

    session.on('close', common.mustCall(() => {
      server.close();
    }));
  }));

  await server.listen();

  const req = await client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port
  });
  if (qlog) req.qlog.pipe(createWriteStream('client.qlog'));

  req.on('stream', common.mustCall(async (stream) => {
    let data = '';
    stream.setEncoding('utf8');

    stream.end('foo');

    for await (const chunk of stream)
      data += chunk;
    strictEqual(data, 'from the server');

    await req.close();
    client.close();
  }));
})().then(common.mustCall());
