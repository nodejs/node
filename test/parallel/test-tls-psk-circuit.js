'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

const CIPHERS = 'PSK+HIGH';
const USERS = {
  UserA: 'd731ef57be09e5204f0b205b60627028',
  UserB: '82072606b502b0f4025e90eb75fe137d'
};
const CLIENT_IDENTITIES = [
  { psk: USERS.UserA, identity: 'UserA' },
  { psk: USERS.UserB, identity: 'UserB' },
  // unrecognized user should fail handshake
  { psk: USERS.UserB, identity: 'UserC' },
  // recognized user but incorrect secret should fail handshake
  { psk: USERS.UserA, identity: 'UserB' },
  { psk: USERS.UserB, identity: 'UserB' }
];
const TEST_DATA = 'Hello from test!';

const clientResults = [];
const connectingIds = [];

const serverOptions = {
  ciphers: CIPHERS,
  pskCallback(id) {
    connectingIds.push(id);
    if (id in USERS) {
      return { psk: USERS[id] };
    }
  }
};

const server = tls.createServer(serverOptions, common.mustCall((c) => {
  c.once('data', (data) => {
    assert.strictEqual(data.toString(), TEST_DATA);
    c.end(data);
  });
}, 3));

server.listen(0, () => {
  startTest(server.address().port);
});

function startTest(port) {
  function connectClient(options, next) {
    const client = tls.connect(port, 'localhost', options, () => {
      clientResults.push('connect');

      client.end(TEST_DATA);

      client.on('data', common.mustCallAtLeast((data) => {
        assert.strictEqual(data.toString(), TEST_DATA);
      }));

      client.on('close', common.mustCall(next));
    });

    let errored = false;
    client.on('error', (err) => {
      if (!errored) {
        clientResults.push(err.message);
        errored = true;
        next();
      }
    });
  }

  function doTestCase(tcnum) {
    if (tcnum >= CLIENT_IDENTITIES.length) {
      server.close();
    } else {
      connectClient({
        ciphers: CIPHERS,
        rejectUnauthorized: false,
        pskCallback: () => CLIENT_IDENTITIES[tcnum]
      }, () => {
        doTestCase(tcnum + 1);
      });
    }
  }
  doTestCase(0);
}

process.on('exit', () => {
  assert.deepStrictEqual(clientResults, [
    'connect',
    'connect',
    'Client network socket disconnected before secure TLS connection was established',
    'Client network socket disconnected before secure TLS connection was established',
    'connect'
  ]);
  assert.deepStrictEqual(connectingIds, [
    'UserA', 'UserB', 'UserC', 'UserB', 'UserB'
  ]);
});
