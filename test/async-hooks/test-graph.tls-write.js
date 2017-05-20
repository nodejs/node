'use strict';

const common = require('../common');
const initHooks = require('./init-hooks');
const verifyGraph = require('./verify-graph');
const fs = require('fs');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

if (!common.hasIPv6) {
  common.skip('IPv6 support required');
  return;
}

const tls = require('tls');
const hooks = initHooks();
hooks.enable();

//
// Creating server and listening on port
//
const server = tls
  .createServer({
    cert: fs.readFileSync(common.fixturesDir + '/test_cert.pem'),
    key: fs.readFileSync(common.fixturesDir + '/test_key.pem')
  })
  .on('listening', common.mustCall(onlistening))
  .on('secureConnection', common.mustCall(onsecureConnection))
  .listen(common.PORT);

function onlistening() {
  //
  // Creating client and connecting it to server
  //
  tls
    .connect(common.PORT, { rejectUnauthorized: false })
    .on('secureConnect', common.mustCall(onsecureConnect));
}

function onsecureConnection() {}

function onsecureConnect() {
  // Destroying client socket
  this.destroy();

  // Closing server
  server.close(common.mustCall(onserverClosed));
}

function onserverClosed() {}

process.on('exit', onexit);

function onexit() {
  hooks.disable();

  verifyGraph(
    hooks,
    [ { type: 'TCPWRAP', id: 'tcp:1', triggerId: null },
      { type: 'TCPWRAP', id: 'tcp:2', triggerId: 'tcp:1' },
      { type: 'TLSWRAP', id: 'tls:1', triggerId: 'tcp:1' },
      { type: 'GETADDRINFOREQWRAP',
        id: 'getaddrinforeq:1', triggerId: 'tls:1' },
      { type: 'TCPCONNECTWRAP',
        id: 'tcpconnect:1', triggerId: 'tcp:2' },
      { type: 'WRITEWRAP', id: 'write:1', triggerId: 'tcpconnect:1' },
      { type: 'TCPWRAP', id: 'tcp:3', triggerId: 'tcp:1' },
      { type: 'TLSWRAP', id: 'tls:2', triggerId: 'tcp:1' },
      { type: 'TIMERWRAP', id: 'timer:1', triggerId: 'tcp:1' },
      { type: 'WRITEWRAP', id: 'write:2', triggerId: null },
      { type: 'WRITEWRAP', id: 'write:3', triggerId: null },
      { type: 'WRITEWRAP', id: 'write:4', triggerId: null },
      { type: 'Immediate', id: 'immediate:1', triggerId: 'tcp:2' },
      { type: 'Immediate', id: 'immediate:2', triggerId: 'tcp:3' } ]
  );
}
