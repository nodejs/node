// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const Countdown = require('../common/countdown');
const assert = require('assert');
const { key, cert, ca } = require('../common/quic');
const { once } = require('events');
const { createQuicSocket } = require('net');

const qlog = process.env.NODE_QLOG === '1';
const { createWriteStream } = require('fs');

const options = { key, cert, ca, alpn: 'zzz', qlog };

const client = createQuicSocket({ qlog, client: options });
const server = createQuicSocket({ qlog, server: options });

const countdown = new Countdown(2, () => {
  server.close();
  client.close();
});

(async function() {
  server.on('session', common.mustCall(async (session) => {
    if (qlog) session.qlog.pipe(createWriteStream('server.qlog'));
    const uni = await session.openStream({ halfOpen: true });
    uni.write('hi', common.mustCall((err) => assert(!err)));
    uni.on('error', common.mustNotCall());
    uni.on('data', common.mustNotCall());
    uni.on('close', common.mustCall());
    uni.close();

    session.on('stream', common.mustCall((stream) => {
      assert(stream.bidirectional);
      assert(stream.readable);
      assert(stream.writable);
      stream.on('close', common.mustCall());
      stream.end();
      stream.resume();
    }));
    session.on('close', common.mustCall());
  }));

  await server.listen();

  const req = await client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port,
  });
  if (qlog) req.qlog.pipe(createWriteStream('client.qlog'));

  req.on('stream', common.mustCall((stream) => {
    assert(stream.unidirectional);
    assert(stream.readable);
    assert(!stream.writable);
    stream.on('data', common.mustCall((chunk) => {
      assert.strictEqual(chunk.toString(), 'hi');
    }));
    stream.on('end', common.mustCall());
    stream.on('close', common.mustCall(() => {
      countdown.dec();
    }));
  }));

  const stream = await req.openStream();
  stream.write('hello', common.mustCall((err) => assert(!err)));
  stream.write('there', common.mustCall((err) => assert(!err)));
  stream.resume();
  stream.on('error', common.mustNotCall());
  stream.on('end', common.mustCall());
  stream.on('close', common.mustCall());
  await stream.close();
  countdown.dec();

  await Promise.all([
    once(server, 'close'),
    once(client, 'close')
  ]);

})().then(common.mustCall());
