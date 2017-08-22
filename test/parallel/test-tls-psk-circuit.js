'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}

const assert = require('assert');
const tls = require('tls');

const CIPHERS = 'PSK+HIGH';
const USERS = {
  UserA: Buffer.from('d731ef57be09e5204f0b205b60627028', 'hex'),
  UserB: Buffer.from('82072606b502b0f4025e90eb75fe137d', 'hex')
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

let serverConnections = 0;
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

const server = tls.createServer(serverOptions, (c) => {
  serverConnections++;
  c.once('data', (data) => {
    assert.strictEqual(data.toString(), TEST_DATA);
    c.end(data);
  });
});
server.listen(common.PORT, startTest);

function startTest() {
  function connectClient(options, next) {
    const client = tls.connect(common.PORT, 'localhost', options, () => {
      clientResults.push('connect');

      client.end(TEST_DATA);

      client.on('data', (data) => {
        assert.strictEqual(data.toString(), TEST_DATA);
      });

      client.on('close', next);
    });
    client.on('error', (err) => {
      clientResults.push(err.message);
      next();
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
  assert.strictEqual(serverConnections, 3);
  assert.deepStrictEqual(clientResults, [
    'connect', 'connect', 'socket hang up', 'socket hang up', 'connect'
  ]);
  assert.deepStrictEqual(connectingIds, [
    'UserA', 'UserB', 'UserC', 'UserB', 'UserB'
  ]);
});
