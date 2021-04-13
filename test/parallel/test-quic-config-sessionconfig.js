// Flags: --no-warnings
'use strict';

const common = require('../common');

if (!common.hasQuic)
  common.skip('quic support is not enabled');

const assert = require('assert');
const {
  SocketAddress,
} = require('net');

const {
  inspect,
} = require('util');

const {
  SessionConfig,
} = require('net/quic');

{
  // Works. No errors.
  const sc = new SessionConfig('client');
  assert(SessionConfig.isSessionConfig(sc));
  assert(!SessionConfig.isSessionConfig({ }));
  assert.strictEqual(sc.side, 'client');
  assert.match(inspect(sc), /SessionConfig {/);
}

{
  // Works. No errors.
  const sc = new SessionConfig('server');
  assert(SessionConfig.isSessionConfig(sc));
  assert(!SessionConfig.isSessionConfig({ }));
  assert.strictEqual(sc.side, 'server');
}

[1, true, {}, [], null].forEach((side) => {
  assert.throws(() => new SessionConfig(side), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

[1, true, 'abc', null].forEach((options) => {
  assert.throws(() => new SessionConfig('client', options), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

[1, true, {}, [], null].forEach((alpn) => {
  assert.throws(() => new SessionConfig('client', { alpn }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

[1, true, {}, [], null].forEach((hostname) => {
  assert.throws(() => new SessionConfig('client', { hostname }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

[1, true, {}, [], null].forEach((dcid) => {
  assert.throws(() => new SessionConfig('client', { dcid }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

[1, true, {}, [], null].forEach((scid) => {
  assert.throws(() => new SessionConfig('client', { scid }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

{
  const cid = Buffer.alloc(21);
  assert.throws(() => new SessionConfig('client', { dcid: cid }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => new SessionConfig('client', { scid: cid }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });

  for (let n = 0; n < 21; n++) {
    const cid = Buffer.alloc(n);
    new SessionConfig('client', { dcid: cid, scid: cid });
  }
}

[
  'qlog',
].forEach((i) => {
  ['abc', 1, {}, [], null, 1.1, Infinity].forEach((n) => {
    assert.throws(() => new SessionConfig({ [i]: n }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });
});

[
  Buffer.alloc(0),
  Buffer.alloc(20),
  new Int8Array(0),
  new Int8Array(20),
  new Int16Array(1),
  new Int32Array(1),
  new BigInt64Array(1),
  new Uint8Array(0),
  new Uint8Array(20),
  new Uint16Array(1),
  new Uint32Array(1),
  new BigUint64Array(1),
  new ArrayBuffer(20),
  new DataView(new ArrayBuffer(5)),
].forEach((dcid) => {
  new SessionConfig('client', { dcid });
});

[
  Buffer.alloc(0),
  Buffer.alloc(20),
  new Int8Array(0),
  new Int8Array(20),
  new Int16Array(1),
  new Int32Array(1),
  new BigInt64Array(1),
  new Uint8Array(0),
  new Uint8Array(20),
  new Uint16Array(1),
  new Uint32Array(1),
  new BigUint64Array(1),
  new ArrayBuffer(20),
  new DataView(new ArrayBuffer(5)),
].forEach((scid) => {
  new SessionConfig('client', { scid });
});

[1, true, {}, null].forEach((preferredAddressStrategy) => {
  assert.throws(() => new SessionConfig({ preferredAddressStrategy }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

{
  new SessionConfig('client', { preferredAddressStrategy: 'use' });
  new SessionConfig('client', { preferredAddressStrategy: 'use' });
}

[1, true, null, 'abc'].forEach((secure) => {
  assert.throws(() => new SessionConfig({ secure }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

[1, true, null, 'abc'].forEach((transportParams) => {
  assert.throws(() => new SessionConfig({ transportParams }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

['abc', 1, {}, [], null].forEach((clientHello) => {
  assert.throws(() => new SessionConfig({ secure: { clientHello } }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

['abc', 1, {}, [], null].forEach((enableTLSTrace) => {
  assert.throws(() => new SessionConfig({ secure: { enableTLSTrace } }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

['abc', 1, {}, [], null].forEach((keylog) => {
  assert.throws(() => new SessionConfig({ secure: { keylog } }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

['abc', {}, [], null].forEach((pskCallback) => {
  assert.throws(() => new SessionConfig({ secure: { pskCallback } }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

['abc', {}, [], null].forEach((rejectUnauthorized) => {
  assert.throws(() => new SessionConfig({ secure: { rejectUnauthorized } }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

['abc', {}, [], null].forEach((requestPeerCertificate) => {
  assert.throws(() => new SessionConfig({
    secure: { requestPeerCertificate },
  }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

['abc', {}, [], null].forEach((requestOCSP) => {
  assert.throws(() => new SessionConfig({ secure: { requestOCSP } }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

['abc', {}, [], null].forEach((verifyHostnameIdentity) => {
  assert.throws(() => new SessionConfig({
    secure: { verifyHostnameIdentity },
  }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

[
  'initialMaxStreamDataBidiLocal',
  'initialMaxStreamDataBidiRemote',
  'initialMaxStreamDataUni',
  'initialMaxData',
  'initialMaxStreamsBidi',
  'initialMaxStreamsUni',
  'maxIdleTimeout',
  'activeConnectionIdLimit',
  'ackDelayExponent',
  'maxAckDelay',
  'maxDatagramFrameSize',
].forEach((i) => {
  new SessionConfig('client', { transportParams: { [i]: 1 } });
  new SessionConfig('client', { transportParams: { [i]: 1n } });

  ['abc', true, {}, []].forEach((n) => {
    assert.throws(() => new SessionConfig('client', {
      transportParams: { [i]: n }
    }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });
});

['abc', 1, {}, [], null].forEach((disableActiveMigration) => {
  assert.throws(() => new SessionConfig('client', {
    transportParams: {
      disableActiveMigration,
    },
  }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

[1, 'abc', true, null].forEach((n) => {
  assert.throws(() => {
    new SessionConfig('client', {
      transportParams: {
        preferredAddress: {
          ipv4: n,
        },
      },
    });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    new SessionConfig('client', {
      transportParams: {
        preferredAddress: {
          ipv6: n,
        },
      },
    });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

new SessionConfig('client', {
  transportParams: {
    preferredAddress: {},
  },
});

new SessionConfig('client', {
  transportParams: {
    preferredAddress: {
      ipv4: {},
      ipv6: {},
    },
  },
});

new SessionConfig('client', {
  transportParams: {
    preferredAddress: {
      ipv4: new SocketAddress(),
      ipv6: new SocketAddress({ family: 'ipv6' }),
    },
  },
});

assert.throws(() => {
  new SessionConfig('client', {
    transportParams: {
      preferredAddress: {
        ipv4: new SocketAddress(),
        ipv6: new SocketAddress(),
      },
    },
  });
}, {
  code: 'ERR_INVALID_ARG_VALUE',
});


{
  const sc = new SessionConfig('client', {
    alpn: 'abc',
    hostname: 'localhost',
    preferredAddressStrategy: 'use',
    qlog: true,
    secure: {
      clientHello: true,
      enableTLSTrace: true,
      keylog: true,
      pskCallback() {},
      rejectUnauthorized: false,
      requestOCSP: false,
      verifyHostnameIdentity: false,
    },
    transportParams: {
      initialMaxStreamDataBidiLocal: 1,
      initialMaxStreamDataBidiRemote: 1,
      initialMaxStreamDataUni: 1,
      initialMaxData: 1,
      initialMaxStreamsBidi: 1,
      initialMaxStreamsUni: 1,
      maxIdleTimeout: 1,
      activeConnectionIdLimit: 1,
      ackDelayExponent: 1,
      maxAckDelay: 1,
      maxDatagramFrameSize: 1,
      disableActiveMigration: false,
      preferredAddress: {
        ipv4: new SocketAddress({
          address: '123.123.123.123',
        }),
        ipv6: new SocketAddress({
          address: '::1',
          family: 'ipv6',
        }),
      }
    },
  });
  assert.strictEqual(sc.side, 'client');
  assert.strictEqual(sc.hostname, 'localhost');
}
