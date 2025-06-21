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
  .createServer(common.mustCall(onconnection))
  .on('listening', common.mustCall(onlistening));

server.listen(0);

net.connect({ port: server.address().port, host: '::1' },
            common.mustCall(onconnected));

function onlistening() {}

function onconnected() {}

function onconnection(c) {
  c.end();
  this.close(common.mustCall(onserverClosed));
}

function onserverClosed() {}

process.on('exit', onexit);

function onexit() {
  hooks.disable();

  verifyGraph(
    hooks,
    [ { type: 'TCPSERVERWRAP', id: 'tcpserver:1', triggerAsyncId: null },
      { type: 'TCPWRAP', id: 'tcp:1', triggerAsyncId: null },
      { type: 'TCPCONNECTWRAP',
        id: 'tcpconnect:1', triggerAsyncId: 'tcp:1' },
      { type: 'TCPWRAP', id: 'tcp:2', triggerAsyncId: 'tcpserver:1' },
      { type: 'SHUTDOWNWRAP', id: 'shutdown:1', triggerAsyncId: 'tcp:2' } ],
  );
}
