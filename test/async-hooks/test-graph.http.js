'use strict';

const common = require('../common');
if (!common.hasIPv6)
  common.skip('IPv6 support required');

const initHooks = require('./init-hooks');
const verifyGraph = require('./verify-graph');
const http = require('http');

const hooks = initHooks();
hooks.enable();

const server = http.createServer(common.mustCall(function(req, res) {
  res.end();
  this.close(common.mustCall());
}));
server.listen(0, common.mustCall(function() {
  http.get({
    host: '::1',
    family: 6,
    port: server.address().port
  }, common.mustCall());
}));

process.on('exit', function() {
  hooks.disable();

  verifyGraph(
    hooks,
    [ { type: 'TCPSERVERWRAP',
        id: 'tcpserver:1',
        triggerAsyncId: null },
      { type: 'TCPWRAP', id: 'tcp:1', triggerAsyncId: 'tcpserver:1' },
      { type: 'TCPCONNECTWRAP',
        id: 'tcpconnect:1',
        triggerAsyncId: 'tcp:1' },
      { type: 'HTTPPARSER',
        id: 'httpparser:1',
        triggerAsyncId: 'tcpserver:1' },
      { type: 'HTTPPARSER',
        id: 'httpparser:2',
        triggerAsyncId: 'tcpserver:1' },
      { type: 'TCPWRAP', id: 'tcp:2', triggerAsyncId: 'tcpserver:1' },
      { type: 'Timeout', id: 'timeout:1', triggerAsyncId: 'tcp:2' },
      { type: 'TIMERWRAP', id: 'timer:1', triggerAsyncId: 'tcp:2' },
      { type: 'HTTPPARSER',
        id: 'httpparser:3',
        triggerAsyncId: 'tcp:2' },
      { type: 'HTTPPARSER',
        id: 'httpparser:4',
        triggerAsyncId: 'tcp:2' },
      { type: 'Timeout',
        id: 'timeout:2',
        triggerAsyncId: 'httpparser:4' },
      { type: 'TIMERWRAP',
        id: 'timer:2',
        triggerAsyncId: 'httpparser:4' },
      { type: 'SHUTDOWNWRAP',
        id: 'shutdown:1',
        triggerAsyncId: 'tcp:2' } ]
  );
});
