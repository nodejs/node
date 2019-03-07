// Covers TCPWRAP and related TCPCONNECTWRAP
'use strict';

const common = require('../common');
if (!common.hasIPv6)
  common.skip('IPv6 support required');

const assert = require('assert');
const tick = require('../common/tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');
const net = require('net');

let tcp1, tcp2;
let tcpserver;
let tcpconnect;

const hooks = initHooks();
hooks.enable();

const server = net
  .createServer(common.mustCall(onconnection))
  .on('listening', common.mustCall(onlistening));

// Calling server.listen creates a TCPWRAP synchronously
{
  server.listen(0);
  const tcpsservers = hooks.activitiesOfTypes('TCPSERVERWRAP');
  const tcpconnects = hooks.activitiesOfTypes('TCPCONNECTWRAP');
  assert.strictEqual(tcpsservers.length, 1);
  assert.strictEqual(tcpconnects.length, 0);
  tcpserver = tcpsservers[0];
  assert.strictEqual(tcpserver.type, 'TCPSERVERWRAP');
  assert.strictEqual(typeof tcpserver.uid, 'number');
  assert.strictEqual(typeof tcpserver.triggerAsyncId, 'number');
  checkInvocations(tcpserver, { init: 1 }, 'when calling server.listen');
}

// Calling net.connect creates another TCPWRAP synchronously
{
  net.connect(
    { port: server.address().port, host: '::1' },
    common.mustCall(onconnected));
  const tcps = hooks.activitiesOfTypes('TCPWRAP');
  assert.strictEqual(tcps.length, 1);
  process.nextTick(() => {
    const tcpconnects = hooks.activitiesOfTypes('TCPCONNECTWRAP');
    assert.strictEqual(tcpconnects.length, 1);
  });

  tcp1 = tcps[0];
  assert.strictEqual(tcps.length, 1);
  assert.strictEqual(tcp1.type, 'TCPWRAP');
  assert.strictEqual(typeof tcp1.uid, 'number');
  assert.strictEqual(typeof tcp1.triggerAsyncId, 'number');

  checkInvocations(tcpserver, { init: 1 },
                   'tcpserver when client is connecting');
  checkInvocations(tcp1, { init: 1 }, 'tcp1 when client is connecting');
}

function onlistening() {
  assert.strictEqual(hooks.activitiesOfTypes('TCPWRAP').length, 1);
}

// Depending on timing we see client: onconnected or server: onconnection first
// Therefore we can't depend on any ordering, but when we see a connection for
// the first time we assign the tcpconnectwrap.
function ontcpConnection(serverConnection) {
  if (tcpconnect != null) {
    // When client receives connection first ('onconnected') and the server
    // second then we see an 'after' here, otherwise not
    const expected = serverConnection ?
      { init: 1, before: 1, after: 1 } :
      { init: 1, before: 1 };
    checkInvocations(
      tcpconnect, expected,
      'tcpconnect: when both client and server received connection');
    return;
  }

  // Only focusing on TCPCONNECTWRAP here
  const tcpconnects = hooks.activitiesOfTypes('TCPCONNECTWRAP');
  assert.strictEqual(tcpconnects.length, 1);
  tcpconnect = tcpconnects[0];
  assert.strictEqual(tcpconnect.type, 'TCPCONNECTWRAP');
  assert.strictEqual(typeof tcpconnect.uid, 'number');
  assert.strictEqual(typeof tcpconnect.triggerAsyncId, 'number');
  // When client receives connection first ('onconnected'), we 'before' has
  // been invoked at this point already, otherwise it only was 'init'ed
  const expected = serverConnection ? { init: 1 } : { init: 1, before: 1 };
  checkInvocations(tcpconnect, expected,
                   'tcpconnect: when tcp connection is established');
}

let serverConnected = false;
function onconnected() {
  ontcpConnection(false);
  // In the case that the client connects before the server TCPWRAP 'before'
  // and 'after' weren't invoked yet. Also @see ontcpConnection.
  const expected = serverConnected ?
    { init: 1, before: 1, after: 1 } :
    { init: 1 };
  checkInvocations(tcpserver, expected, 'tcpserver when client connects');
  checkInvocations(tcp1, { init: 1 }, 'tcp1 when client connects');
}

function onconnection(c) {
  serverConnected = true;
  ontcpConnection(true);

  const tcps = hooks.activitiesOfTypes([ 'TCPWRAP' ]);
  const tcpconnects = hooks.activitiesOfTypes('TCPCONNECTWRAP');
  assert.strictEqual(tcps.length, 2);
  assert.strictEqual(tcpconnects.length, 1);
  tcp2 = tcps[1];
  assert.strictEqual(tcp2.type, 'TCPWRAP');
  assert.strictEqual(typeof tcp2.uid, 'number');
  assert.strictEqual(typeof tcp2.triggerAsyncId, 'number');

  checkInvocations(tcpserver, { init: 1, before: 1 },
                   'tcpserver when server receives connection');
  checkInvocations(tcp1, { init: 1 }, 'tcp1 when server receives connection');
  checkInvocations(tcp2, { init: 1 }, 'tcp2 when server receives connection');

  c.end();
  this.close(common.mustCall(onserverClosed));
}

function onserverClosed() {
  checkInvocations(tcpserver, { init: 1, before: 1, after: 1, destroy: 1 },
                   'tcpserver when server is closed');
  setImmediate(() => {
    checkInvocations(tcp1, { init: 1, before: 2, after: 2, destroy: 1 },
                     'tcp1 after server is closed');
  });
  checkInvocations(tcp2, { init: 1, before: 1, after: 1 },
                   'tcp2 synchronously when server is closed');

  tick(2, () => {
    checkInvocations(tcp2, { init: 1, before: 2, after: 2, destroy: 1 },
                     'tcp2 when server is closed');
    checkInvocations(tcpconnect, { init: 1, before: 1, after: 1, destroy: 1 },
                     'tcpconnect when server is closed');
  });
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck([ 'TCPWRAP', 'TCPSERVERWRAP', 'TCPCONNECTWRAP' ]);

  checkInvocations(tcpserver, { init: 1, before: 1, after: 1, destroy: 1 },
                   'tcpserver when process exits');
  checkInvocations(
    tcp1, { init: 1, before: 2, after: 2, destroy: 1 },
    'tcp1 when process exits');
  checkInvocations(
    tcp2, { init: 1, before: 2, after: 2, destroy: 1 },
    'tcp2 when process exits');
  checkInvocations(
    tcpconnect, { init: 1, before: 1, after: 1, destroy: 1 },
    'tcpconnect when process exits');
}
