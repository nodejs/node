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

const server = new Endpoint();
const client = new Endpoint();

async function getLength(stream) {
  let length = 0;
  for await (const chunk of stream.readableWebStream())
    length += chunk.byteLength;
  return length;
}

server.onsession = common.mustCall(async ({ session }) => {
  await session.handshake;
  const bidi = session.open({ body: 'hello' });
  assert.strictEqual(await getLength(bidi), 5);
  assert(!bidi.unidirectional);
  await bidi.closed;

  const uni = session.open({ unidirectional: true, body: 'what' });
  assert(uni.unidirectional);
  await uni.closed;

  client.close();
  server.close();
});

server.listen({
  alpn: 'zzz',
  secure: {
    key: fixtures.readKey('rsa_private.pem'),
    cert: fixtures.readKey('rsa_cert.crt'),
  }
});

const req = client.connect(server.address, { alpn: 'zzz' });

req.onstream = common.mustCall(async ({ stream, respondWith }) => {
  assert(stream.serverInitiated);
  switch (stream.id) {
    case 1n:
      assert(!stream.unidirectional);
      respondWith({ body: 'there' });
      assert.strictEqual(await getLength(stream), 5);
      break;
    case 3n:
      assert(stream.unidirectional);
      assert.strictEqual(respondWith, undefined);
      assert.strictEqual(await getLength(stream), 4);
      break;
  }
}, 2);
