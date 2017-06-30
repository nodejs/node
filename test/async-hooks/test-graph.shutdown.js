'use strict';

const common = require('../common');
if (!common.hasIPv6)
  common.skip('IPv6 support required');

const initHooks = require('./init-hooks');
const verifyGraph = require('./verify-graph');
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
    [ { type: 'TCPWRAP', id: 'tcp:1', triggerAsyncId: null },
      { type: 'TCPWRAP', id: 'tcp:2', triggerAsyncId: 'tcp:1' },
      { type: 'GETADDRINFOREQWRAP',
        id: 'getaddrinforeq:1', triggerAsyncId: 'tcp:2' },
      { type: 'TCPCONNECTWRAP',
        id: 'tcpconnect:1', triggerAsyncId: 'tcp:2' },
      { type: 'TCPWRAP', id: 'tcp:3', triggerAsyncId: 'tcp:1' },
      { type: 'SHUTDOWNWRAP', id: 'shutdown:1', triggerAsyncId: 'tcp:3' } ]
  );
}
