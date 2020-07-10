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

const options = { key, cert, ca, alpn: 'zzz' };

const client = createQuicSocket({ client: options });
const server = createQuicSocket({ server: options });

const countdown = new Countdown(2, () => {
  server.close();
  client.close();
});

(async function() {
  server.on('session', common.mustCall((session) => {
    session.on('secure', common.mustCall((servername, alpn, cipher) => {
      const uni = session.openStream({ halfOpen: true });
      uni.write('hi', common.expectsError());
      uni.on('error', common.mustCall(() => {
        assert.strictEqual(uni.aborted, true);
      }));
      uni.on('data', common.mustNotCall());
      uni.on('close', common.mustCall());
      uni.close(3);
    }));
    session.on('stream', common.mustNotCall());
    session.on('close', common.mustCall());
  }));

  await server.listen();

  const req = await client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port,
  });

  req.on('secure', common.mustCall((servername, alpn, cipher) => {
    const stream = req.openStream();
    stream.write('hello', common.expectsError());
    stream.write('there', common.expectsError());
    stream.on('error', common.mustCall(() => {
      assert.strictEqual(stream.aborted, true);
    }));
    stream.on('end', common.mustNotCall());
    stream.on('close', common.mustCall(() => {
      countdown.dec();
    }));
    stream.close(1);
  }));

  req.on('stream', common.mustCall((stream) => {
    stream.on('abort', common.mustNotCall());
    stream.on('data', common.mustCall((chunk) => {
      assert.strictEqual(chunk.toString(), 'hi');
    }));
    stream.on('end', common.mustCall());
    stream.on('close', common.mustCall(() => {
      countdown.dec();
    }));
  }));

  await Promise.all([
    once(server, 'close'),
    once(client, 'close')
  ]);

})().then(common.mustCall());
