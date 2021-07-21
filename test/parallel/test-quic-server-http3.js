// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');

if (!common.hasQuic)
  common.skip('quic support is not enabled');

const {
  Endpoint,
} = require('net/quic');

const {
  TextDecoderStream,
} = require('stream/web');

const fixtures = require('../common/fixtures');

const endpoint = new Endpoint({ address: { port: 0 } });

endpoint.onsession = common.mustCall(({ session }) => {
  session.onstream = common.mustCall(async ({ stream, respondWith }) => {
    respondWith({
      headers: { 'content-type': 'text/plain' },
      trailers: function() {
        return new Promise((res) => {
          setTimeout(() => res({ abc: 2 }), 100);
        });
      },
      body: 'right back at you',
    });

    stream.trailers.then(common.mustCall((trailers) => {
      assert.strictEqual(trailers.abc, '123');
    }));

    const headers = await stream.headers;
    assert.strictEqual(headers[':method'], 'POST');
    assert.strictEqual(headers[':path'], '/');
    assert.strictEqual(
      headers[':authority'],
      `localhost:${endpoint.address.port}`);
    assert.strictEqual(headers[':scheme'], 'https');

    let data = '';
    const readable = stream.readableNodeStream();
    readable.setEncoding('utf8');
    readable.on('data', (chunk) => data += chunk);
    readable.on('close', common.mustCall(() => {
      assert.strictEqual(data, 'hello there');
    }));
  });

  session.handshake.then(() => {
    assert(session.datagram('hello'));
    assert(!session.datagram('hello'.repeat(3)));
  });
});

endpoint.listen({
  secure: {
    key: fixtures.readKey('rsa_private.pem'),
    cert: fixtures.readKey('rsa_cert.crt'),
  },
});

// Client....

(async () => {
  const client = new Endpoint();

  const req = client.connect(
    endpoint.address,
    {
      hostname: 'localhost',
      transportParams: {
        maxDatagramFrameSize: 10,
      },
    });

  req.ondatagram = common.mustCall(({ datagram }) => {
    assert.strictEqual(Buffer.from(datagram).toString(), 'hello');
  });

  // Since we're not using early data, wait for the completion of
  // the TLS handshake before we open a stream...
  await req.handshake;

  const stream = req.open({
    headers: { ':method': 'POST' },
    trailers: { abc: '123' },
    body: 'hello there',
  });

  const readable =
    stream.readableWebStream().pipeThrough(new TextDecoderStream());
  let data = '';
  for await (const chunk of readable)
    data += chunk;
  assert.strictEqual(data, 'right back at you');

  // Wait for the stream to be closed before we do anything else.
  await stream.closed;

  stream.trailers.then(common.mustCall((trailers) => {
    assert.strictEqual(trailers.abc, '2');
  }));

  const headers = await stream.headers;
  assert.strictEqual(headers[':status'], 200);
  assert.strictEqual(headers['content-type'], 'text/plain');

  await Promise.all([
    client.close(),
    endpoint.close(),
  ]);
})().then(common.mustCall());
