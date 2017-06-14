'use strict';

const common = require('../common');
const initHooks = require('./init-hooks');
const verifyGraph = require('./verify-graph');

const net = require('net');

common.refreshTmpDir();

const hooks = initHooks();
hooks.enable();

net.createServer(function(c) {
  c.end();
  this.close();
}).listen(common.PIPE, common.mustCall(onlisten));

function onlisten() {
  net.connect(common.PIPE, common.mustCall(onconnect));
}

function onconnect() {}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  verifyGraph(
    hooks,
    [ { type: 'PIPEWRAP', id: 'pipe:1', triggerAsyncId: null },
      { type: 'PIPEWRAP', id: 'pipe:2', triggerAsyncId: 'pipe:1' },
      { type: 'PIPECONNECTWRAP', id: 'pipeconnect:1',
        triggerAsyncId: 'pipe:2' },
      { type: 'PIPEWRAP', id: 'pipe:3', triggerAsyncId: 'pipe:1' },
      { type: 'SHUTDOWNWRAP', id: 'shutdown:1', triggerAsyncId: 'pipe:3' } ]
  );
}
