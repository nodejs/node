// Flags: --no-warnings
'use strict';

const common = require('../common');

if (!common.hasQuic)
  common.skip('quic support is not enabled');

const assert = require('assert');
const {
  Endpoint,
} = require('net/quic');

const fixtures = require('../common/fixtures');
const Countdown = require('../common/countdown');

const server = new Endpoint();
const client = new Endpoint();

const dec = new TextDecoder();

const countdown = new Countdown(2, () => {
  server.close();
  client.close();
});

server.onsession = common.mustCall(async ({ session }) => {
  await session.handshake;
  session.ondatagram = common.mustCall(({ datagram }) => {
    assert.strictEqual(dec.decode(datagram), 'there');
    countdown.dec();
  });
  session.datagram('hello');
});

server.listen({
  alpn: 'zzz',
  secure: {
    key: fixtures.readKey('rsa_private.pem'),
    cert: fixtures.readKey('rsa_cert.crt'),
  }
});

const req = client.connect(server.address, { alpn: 'zzz' });

(async () => {
  await req.handshake;
  req.ondatagram = common.mustCall(({ datagram }) => {
    assert.strictEqual(dec.decode(datagram), 'hello');
    countdown.dec();
  });
  req.datagram('there');

  // Does not send successfully because it is too large
  assert(!req.datagram('there'.repeat(500)));

  // Does not send because it's too small
  assert(!req.datagram(''));
  assert(!req.datagram(Buffer.alloc(0)));

  // Wrong type
  [1, {}, [], false].forEach((i) => {
    assert.throws(() => req.datagram(i), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });

})().then(common.mustCall());
