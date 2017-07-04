'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const initHooks = require('./init-hooks');
const verifyGraph = require('./verify-graph');

const tls = require('tls');
const Connection = process.binding('crypto').Connection;
const hooks = initHooks();
hooks.enable();

function createServerConnection(
  onhandshakestart,
  certificate = null,
  isServer = true,
  servername = 'some server',
  rejectUnauthorized
) {
  if (certificate == null) certificate = tls.createSecureContext();
  const ssl = new Connection(
    certificate.context, isServer, servername, rejectUnauthorized
  );
  if (isServer) {
    ssl.onhandshakestart = onhandshakestart;
    ssl.lastHandshakeTime = 0;
  }
  return ssl;
}

// creating first server connection and start it
const sc1 = createServerConnection(common.mustCall(onfirstHandShake));
sc1.start();

function onfirstHandShake() {
  // Create second connection inside handshake of first to show
  // that the triggerAsyncId of the second will be set to id of the first
  const sc2 = createServerConnection(common.mustCall(onsecondHandShake));
  sc2.start();
}
function onsecondHandShake() { }

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  verifyGraph(
    hooks,
    [ { type: 'CONNECTION', id: 'connection:1', triggerAsyncId: null },
      { type: 'CONNECTION', id: 'connection:2',
        triggerAsyncId: 'connection:1' } ]
  );
}
