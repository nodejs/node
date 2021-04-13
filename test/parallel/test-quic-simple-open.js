// Flags: --no-warnings
'use strict';

// Test the various ways of providing a body on stream open

const common = require('../common');

if (!common.hasQuic)
  common.skip('quic support is not enabled');

const { Blob } = require('buffer');
const assert = require('assert');

const {
  open,
} = require('fs/promises');

const {
  statSync,
  createReadStream,
} = require('fs');

const {
  Endpoint,
} = require('net/quic');

const {
  setTimeout: sleep,
} = require('timers/promises');

const fixtures = require('../common/fixtures');

const {
  size: thisFileSize,
} = statSync(__filename);

const server = new Endpoint();
const client = new Endpoint();

async function getLength(stream) {
  let length = 0;
  assert(!stream.locked);
  for await (const chunk of stream.readableWebStream())
    length += chunk.byteLength;
  assert(stream.locked);
  return length;
}

const kResponses = [
  async (stream) => {
    const length = await getLength(stream);
    assert.strictEqual(length, thisFileSize);
  },
  async (stream) => {
    const length = await getLength(stream);
    assert.strictEqual(length, 5);
  },
  async (stream) => {
    const length = await getLength(stream);
    assert.strictEqual(length, 5);
  },
  async (stream) => {
    const length = await getLength(stream);
    assert.strictEqual(length, 10);
  },
  async (stream) => {
    const length = await getLength(stream);
    assert.strictEqual(length, 5);
  },
  async (stream) => {
    const length = await getLength(stream);
    assert.strictEqual(length, 5);
  },
  async (stream) => {
    const length = await getLength(stream);
    assert.strictEqual(length, 5);
  },
  async (stream) => {
    const length = await getLength(stream);
    assert.strictEqual(length, 5);
  },
  async (stream) => {
    const length = await getLength(stream);
    assert.strictEqual(length, thisFileSize);
  },
  async (stream) => {
    const length = await getLength(stream);
    assert.strictEqual(length, thisFileSize);
  },
  async (stream) => {
    const length = await getLength(stream);
    assert.strictEqual(length, thisFileSize);
  },
  async (stream) => {
    const length = await getLength(stream);
    assert.strictEqual(length, 5);
  },
];

server.onsession = common.mustCall(({ session }) => {
  session.onstream = common.mustCall(({ stream, respondWith }) => {
    assert(stream.unidirectional);
    assert(!stream.serverInitiated);
    assert.strictEqual(respondWith, undefined);
    kResponses.shift()(stream);
  }, kResponses.length);
});

server.listen({
  alpn: 'zzz',
  secure: {
    key: fixtures.readKey('rsa_private.pem'),
    cert: fixtures.readKey('rsa_cert.crt'),
  },
  transportParams: {
    initialMaxStreamsUni: kResponses.length,
  }
});

const req = client.connect(server.address, { alpn: 'zzz' });

async function request(body) {
  await req.handshake;
  const stream = req.open({ unidirectional: true, body });
  await stream.closed;
}

(async () => {
  await Promise.all([
    request(function* () { yield 'hello'; }),
    request(async function* () { yield 'hello'; }),
    request((function* () { yield 'hello'; })()),
    request((async function* () { yield 'hello'; })()),
    request(open(__filename)),
    request(await open(__filename)),
    request(createReadStream(__filename)),
    request((await open(__filename)).readableWebStream()),
    request('hello'),
    request(new Uint8Array(5)),
    request(() => new Blob(['hello', 'there'])),
    request(async () => {
      await sleep(10);
      return 'hello';
    }),
  ]);

  client.close();
  server.close();
})().then(common.mustCall());
