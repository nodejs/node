'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (!common.hasIPv6)
  common.skip('IPv6 support required');

const initHooks = require('./init-hooks');
const verifyGraph = require('./verify-graph');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const hooks = initHooks();
hooks.enable();

//
// Creating server and listening on port
//
const server = tls
  .createServer({
    cert: fixtures.readKey('rsa_cert.crt'),
    key: fixtures.readKey('rsa_private.pem'),
  })
  .on('listening', common.mustCall(onlistening))
  .on('secureConnection', common.mustCall(onsecureConnection))
  .listen(0);

function onlistening() {
  //
  // Creating client and connecting it to server
  //
  tls
    .connect(server.address().port, { rejectUnauthorized: false })
    .on('secureConnect', common.mustCall(onsecureConnect));
}

function onsecureConnection() {}

function onsecureConnect() {
  // end() client socket, which causes slightly different hook events than
  // destroy(), but with TLS1.3 destroy() rips the connection down before the
  // server completes the handshake.
  this.end();

  // Closing server
  server.close(common.mustCall(onserverClosed));
}

function onserverClosed() {}

process.on('exit', onexit);

function onexit() {
  hooks.disable();

  verifyGraph(
    hooks,
    [ { type: 'TCPSERVERWRAP', id: 'tcpserver:1', triggerAsyncId: null },
      { type: 'TCPWRAP', id: 'tcp:1', triggerAsyncId: 'tcpserver:1' },
      { type: 'TLSWRAP', id: 'tls:1', triggerAsyncId: 'tcpserver:1' },
      { type: 'GETADDRINFOREQWRAP',
        id: 'getaddrinforeq:1', triggerAsyncId: 'tls:1' },
      { type: 'TCPCONNECTWRAP',
        id: 'tcpconnect:1', triggerAsyncId: 'tcp:1' },
      { type: 'TCPWRAP', id: 'tcp:2', triggerAsyncId: 'tcpserver:1' },
      { type: 'TLSWRAP', id: 'tls:2', triggerAsyncId: 'tcpserver:1' },
    ],
  );
}
