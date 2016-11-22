'use strict';

const common = require('../common');
const initHooks = require('./init-hooks');
const verifyGraph = require('./verify-graph');

if (!common.hasIPv6) {
  common.skip('IPv6 support required');
  return;
}

const net = require('net');

const hooks = initHooks();
hooks.enable();

const server = net
  .createServer(onconnection)
  .on('listening', common.mustCall(onlistening));
server.listen();
function onlistening() {
  net.connect(server.address().port, common.mustCall(onconnected));
}

function onconnection(c) {
  c.end();
  this.close(onserverClosed);
}

function onconnected() {}

function onserverClosed() {}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  verifyGraph(
    hooks,
    [ { type: 'TCPWRAP', id: 'tcp:1', triggerId: null },
      { type: 'TCPWRAP', id: 'tcp:2', triggerId: 'tcp:1' },
      { type: 'GETADDRINFOREQWRAP',
        id: 'getaddrinforeq:1', triggerId: 'tcp:2' },
      { type: 'TCPCONNECTWRAP',
        id: 'tcpconnect:1', triggerId: 'tcp:2' },
      { type: 'TCPWRAP', id: 'tcp:3', triggerId: 'tcp:1' },
      { type: 'SHUTDOWNWRAP', id: 'shutdown:1', triggerId: 'tcp:3' } ]
  );
}
