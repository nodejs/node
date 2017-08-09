'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const initHooks = require('./init-hooks');
const fixtures = require('../common/fixtures');
const { checkInvocations } = require('./hook-checks');
const tls = require('tls');

const hooks = initHooks();
hooks.enable();

//
// Creating server and listening on port
//
const server = tls
  .createServer({
    cert: fixtures.readSync('test_cert.pem'),
    key: fixtures.readSync('test_key.pem')
  })
  .on('listening', common.mustCall(onlistening))
  .on('secureConnection', common.mustCall(onsecureConnection))
  .listen(common.PORT);

assert.strictEqual(hooks.activitiesOfTypes('WRITEWRAP').length, 0);

function onlistening() {
  assert.strictEqual(hooks.activitiesOfTypes('WRITEWRAP').length, 0);
  //
  // Creating client and connecting it to server
  //
  tls
    .connect(common.PORT, { rejectUnauthorized: false })
    .on('secureConnect', common.mustCall(onsecureConnect));

  assert.strictEqual(hooks.activitiesOfTypes('WRITEWRAP').length, 0);
}

function checkDestroyedWriteWraps(n, stage) {
  const as = hooks.activitiesOfTypes('WRITEWRAP');
  assert.strictEqual(as.length, n, `${n} WRITEWRAPs when ${stage}`);

  function checkValidWriteWrap(w) {
    assert.strictEqual(w.type, 'WRITEWRAP');
    assert.strictEqual(typeof w.uid, 'number');
    assert.strictEqual(typeof w.triggerAsyncId, 'number');

    checkInvocations(w, { init: 1 }, `when ${stage}`);
  }
  as.forEach(checkValidWriteWrap);
}

function onsecureConnection() {
  //
  // Server received client connection
  //
  checkDestroyedWriteWraps(3, 'server got secure connection');
}

function onsecureConnect() {
  //
  // Client connected to server
  //
  checkDestroyedWriteWraps(4, 'client connected');

  //
  // Destroying client socket
  //
  this.destroy();

  checkDestroyedWriteWraps(4, 'client destroyed');

  //
  // Closing server
  //
  server.close(common.mustCall(onserverClosed));
  checkDestroyedWriteWraps(4, 'server closing');
}

function onserverClosed() {
  checkDestroyedWriteWraps(4, 'server closed');
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('WRITEWRAP');
  checkDestroyedWriteWraps(4, 'process exits');
}
