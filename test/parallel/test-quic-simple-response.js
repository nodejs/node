// Flags: --no-warnings
'use strict';

// Tests the various ways of responding with different types of bodies

const common = require('../common');

if (!common.hasQuic)
  common.skip('quic support is not enabled');

const { Blob } = require('buffer');
const assert = require('assert');

const {
  setTimeout: sleep,
} = require('timers/promises');

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
  size: thisFileSize,
} = statSync(__filename);

const fixtures = require('../common/fixtures');

const server = new Endpoint();
const client = new Endpoint();

const kResponses = [
  (stream, respondWith) => {
    assert(stream.unidirectional);
    assert.strictEqual(respondWith, undefined);
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith();
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith({ body: 'hello' });
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith({ body: Buffer.from('hellothere') });
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith({ body: new ArrayBuffer(4) });
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith({ body: new DataView(new ArrayBuffer(4)) });
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith({ body: new Blob(['hello', 'there']) });
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith({ body: new Uint8Array(5000) });
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith(Promise.resolve());
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith({ body: Promise.resolve() });
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith({ body: 'hello' });
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith({ body: () => 'hello there' });
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith({ body: async () => 'hello there!' });
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith({ body: async () => {
      await sleep(10);
      return 'hello there!!';
    } });
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    const filePromise = open(__filename);
    respondWith({ body: filePromise });
    stream.closed.then(common.mustCall(() => {
      filePromise.then(common.mustCall((file) => file.close()));
    }));
  },
  async (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    const file = await open(__filename);
    respondWith({ body: file });
    await stream.closed;
    file.close();
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith({ body: createReadStream(__filename) });
  },
  async (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    const file = await open(__filename);
    respondWith({ body: file.readableWebStream() });
    await stream.closed;
    file.close();
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith({ body: (async function*() { yield 'hello'; })() });
  },
  (stream, respondWith) => {
    assert(!stream.unidirectional);
    assert.strictEqual(typeof respondWith, 'function');
    respondWith({ body: (function*() { yield 'hello'; })() });
  },
];

server.onsession = common.mustCall(async ({ session }) => {
  // TODO(@jasnell): Fix, crashes if done before handshake
  // session.updateKey();
  session.onstream = common.mustCall(({ stream, respondWith }) => {
    kResponses.shift()(stream, respondWith);
  }, kResponses.length);
  await session.handshake;
  session.updateKey();
  assert.strictEqual(session.alpn, 'zzz');
  assert.strictEqual(session.cipher.name, 'TLS_AES_128_GCM_SHA256');
  assert.strictEqual(session.cipher.version, 'TLSv1.3');
  assert(session.earlyData);
  assert.strictEqual(session.ephemeralKeyInfo, undefined);
  assert.strictEqual(session.peerCertificate, undefined);
  assert.strictEqual(
    session.certificate.fingerprint,
    '20:4D:CD:ED:43:8F:83:25:73:59:38:55:9D:20:3F:12:21:D5:1C:A0');
  assert(session.servername, 'localhost');
  assert.deepStrictEqual(session.remoteAddress, client.address);
  // Returns the same object every time
  assert.strictEqual(session.remoteAddress, session.remoteAddress);
  assert.strictEqual(session.certificate, session.certificate);
});

server.listen({
  alpn: 'zzz',
  secure: {
    key: fixtures.readKey('rsa_private.pem'),
    cert: fixtures.readKey('rsa_cert.crt'),
  },
});

const req = client.connect(server.address, {
  alpn: 'zzz',
  hostname: 'localhost',
});

assert.strictEqual(req.alpn, undefined);
assert.strictEqual(req.cipher, undefined);
assert.strictEqual(req.peerCertificate, undefined);
assert.strictEqual(req.validationError, undefined);
assert.strictEqual(req.servername, undefined);
assert.deepStrictEqual(req.ephemeralKeyInfo, {});
assert.deepStrictEqual(req.remoteAddress, server.address);

req.handshake.then(common.mustCall(() => {
  assert.strictEqual(req.alpn, 'zzz');
  assert(!req.earlyData);
  assert.strictEqual(req.cipher.name, 'TLS_AES_128_GCM_SHA256');
  assert.strictEqual(req.cipher.version, 'TLSv1.3');
  assert.strictEqual(req.certificate, undefined);
  assert.strictEqual(
    req.peerCertificate.fingerprint,
    '20:4D:CD:ED:43:8F:83:25:73:59:38:55:9D:20:3F:12:21:D5:1C:A0');
  assert.strictEqual(req.ephemeralKeyInfo.type, 'ECDH');
  assert.strictEqual(req.ephemeralKeyInfo.name, 'prime256v1');
  assert.strictEqual(req.ephemeralKeyInfo.size, 256);

  assert.strictEqual(
    req.validationError.reason,
    'self signed certificate');
  assert.strictEqual(
    req.validationError.code,
    'DEPTH_ZERO_SELF_SIGNED_CERT');
  assert.strictEqual(req.servername, 'localhost');
  assert.deepStrictEqual(req.remoteAddress, server.address);
  assert.strictEqual(req.peerCertificate, req.peerCertificate);
}));

async function request(testExpected) {
  await req.handshake;
  const stream = req.open({ body: 'hello' });
  const chunks = [];
  for await (const chunk of stream.readableWebStream())
    chunks.push(chunk);
  testExpected(await (new Blob(chunks)).arrayBuffer());
}

async function uniRequest(testExpected) {
  await req.handshake;
  const stream = req.open({ unidirectional: true, body: 'hello' });
  await stream.closed;
}

(async () => {
  await Promise.all([
    uniRequest(),
    request((ab) => {
      assert.strictEqual(ab.byteLength, 0);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, 5);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, 10);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, 4);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, 4);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, 10);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, 5000);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, 0);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, 0);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, 5);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, 11);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, 12);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, 13);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, thisFileSize);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, thisFileSize);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, thisFileSize);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, thisFileSize);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, 5);
    }),
    request((ab) => {
      assert.strictEqual(ab.byteLength, 5);
    }),
  ]);

  client.close();
  server.close();
})().then(common.mustCall());
