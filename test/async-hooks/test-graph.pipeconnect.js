'use strict';

const common = require('../common');
const initHooks = require('./init-hooks');
const verifyGraph = require('./verify-graph');

const net = require('net');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const hooks = initHooks();
hooks.enable();

const server = net.createServer((c) => {
  c.end();
  server.close();
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
    [ { type: 'PIPESERVERWRAP', id: 'pipeserver:1', triggerAsyncId: null },
      { type: 'PIPEWRAP', id: 'pipe:1', triggerAsyncId: 'pipeserver:1' },
      { type: 'PIPECONNECTWRAP', id: 'pipeconnect:1',
        triggerAsyncId: 'pipe:1' },
      { type: 'PIPEWRAP', id: 'pipe:2', triggerAsyncId: 'pipeserver:1' },
      { type: 'SHUTDOWNWRAP', id: 'shutdown:1', triggerAsyncId: 'pipe:2' } ]
  );
}
