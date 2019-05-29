'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const tls = require('tls');

const tick = require('../common/tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const hooks = initHooks();
hooks.enable();

// TODO(@sam-github) assumes server handshake completes before client, true for
// 1.2, not for 1.3. Might need a rewrite for TLS1.3.
tls.DEFAULT_MAX_VERSION = 'TLSv1.2';

//
// Creating server and listening on port
//
const server = tls
  .createServer({
    cert: fixtures.readKey('rsa_cert.crt'),
    key: fixtures.readKey('rsa_private.pem')
  })
  .on('listening', common.mustCall(onlistening))
  .on('secureConnection', common.mustCall(onsecureConnection))
  .listen(0);

let svr, client;
function onlistening() {
  //
  // Creating client and connecting it to server
  //
  tls
    .connect(server.address().port, { rejectUnauthorized: false })
    .on('secureConnect', common.mustCall(onsecureConnect));

  const as = hooks.activitiesOfTypes('TLSWRAP');
  assert.strictEqual(as.length, 1);
  svr = as[0];

  assert.strictEqual(svr.type, 'TLSWRAP');
  assert.strictEqual(typeof svr.uid, 'number');
  assert.strictEqual(typeof svr.triggerAsyncId, 'number');
  checkInvocations(svr, { init: 1 }, 'server: when client connecting');
}

function onsecureConnection() {
  //
  // Server received client connection
  //
  const as = hooks.activitiesOfTypes('TLSWRAP');
  assert.strictEqual(as.length, 2);
  // TODO(@sam-github) This happens after onsecureConnect, with TLS1.3.
  client = as[1];
  assert.strictEqual(client.type, 'TLSWRAP');
  assert.strictEqual(typeof client.uid, 'number');
  assert.strictEqual(typeof client.triggerAsyncId, 'number');

  // TODO(thlorenz) which callback did the server wrap execute that already
  // finished as well?
  checkInvocations(svr, { init: 1, before: 1, after: 1 },
                   'server: when server has secure connection');

  checkInvocations(client, { init: 1, before: 2, after: 1 },
                   'client: when server has secure connection');
}

function onsecureConnect() {
  //
  // Client connected to server
  //
  checkInvocations(svr, { init: 1, before: 2, after: 1 },
                   'server: when client connected');
  checkInvocations(client, { init: 1, before: 2, after: 2 },
                   'client: when client connected');

  //
  // Destroying client socket
  //
  this.destroy();  // This destroys client before server handshakes, with TLS1.3
  checkInvocations(svr, { init: 1, before: 2, after: 1 },
                   'server: when destroying client');
  checkInvocations(client, { init: 1, before: 2, after: 2 },
                   'client: when destroying client');

  tick(5, tick1);
  function tick1() {
    checkInvocations(svr, { init: 1, before: 2, after: 2 },
                     'server: when client destroyed');
    // TODO: why is client not destroyed here even after 5 ticks?
    // or could it be that it isn't actually destroyed until
    // the server is closed?
    if (client.before.length < 3) {
      tick(5, tick1);
      return;
    }
    checkInvocations(client, { init: 1, before: 3, after: 3 },
                     'client: when client destroyed');
    //
    // Closing server
    //
    server.close(common.mustCall(onserverClosed));
    // No changes to invocations until server actually closed below
    checkInvocations(svr, { init: 1, before: 2, after: 2 },
                     'server: when closing server');
    checkInvocations(client, { init: 1, before: 3, after: 3 },
                     'client: when closing server');
  }
}

function onserverClosed() {
  //
  // Server closed
  //
  tick(1E4, common.mustCall(() => {
    checkInvocations(svr, { init: 1, before: 2, after: 2 },
                     'server: when server closed');
    checkInvocations(client, { init: 1, before: 3, after: 3 },
                     'client: when server closed');
  }));
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('TLSWRAP');

  checkInvocations(svr, { init: 1, before: 2, after: 2 },
                   'server: when process exits');
  checkInvocations(client, { init: 1, before: 3, after: 3 },
                   'client: when process exits');
}
