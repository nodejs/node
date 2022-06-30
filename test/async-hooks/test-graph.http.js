'use strict';

const common = require('../common');
if (!common.hasIPv6)
  common.skip('IPv6 support required');

const initHooks = require('./init-hooks');
const verifyGraph = require('./verify-graph');
const http = require('http');

const hooks = initHooks();
hooks.enable();

const server = http.createServer(common.mustCall((req, res) => {
  res.end();
  server.close(common.mustCall());
}));
server.listen(0, common.mustCall(() => {
  http.get({
    host: '::1',
    family: 6,
    port: server.address().port
  }, common.mustCall());
}));

process.on('exit', () => {
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
      { type: 'HTTPCLIENTREQUEST',
        id: 'httpclientrequest:1',
        triggerAsyncId: 'tcpserver:1' },
      { type: 'TCPWRAP', id: 'tcp:2', triggerAsyncId: 'tcpserver:1' },
      { type: 'HTTPINCOMINGMESSAGE',
        id: 'httpincomingmessage:1',
        triggerAsyncId: 'tcp:2' },
      { type: 'Timeout',
        id: 'timeout:1',
        triggerAsyncId: null },
      { type: 'SHUTDOWNWRAP',
        id: 'shutdown:1',
        triggerAsyncId: 'tcp:2' } ]
  );
});
