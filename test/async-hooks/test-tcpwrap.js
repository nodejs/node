// Covers TCPWRAP and related TCPCONNECTWRAP
'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('./tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

if (!common.hasIPv6) {
  common.skip('IPv6 support required');
  return;
}

const net = require('net');

let tcp1, tcp2, tcp3;
let tcpconnect;

const hooks = initHooks();
hooks.enable();

const server = net
  .createServer(common.mustCall(onconnection))
  .on('listening', common.mustCall(onlistening));

// Calling server.listen creates a TCPWRAP synchronously
{
  server.listen(common.PORT);
  const tcps = hooks.activitiesOfTypes('TCPWRAP');
  const tcpconnects = hooks.activitiesOfTypes('TCPCONNECTWRAP');
  assert.strictEqual(
    tcps.length, 1,
    'one TCPWRAP created synchronously when calling server.listen');
  assert.strictEqual(
    tcpconnects.length, 0,
    'no TCPCONNECTWRAP created synchronously when calling server.listen');
  tcp1 = tcps[0];
  assert.strictEqual(tcp1.type, 'TCPWRAP', 'tcp wrap');
  assert.strictEqual(typeof tcp1.uid, 'number', 'uid is a number');
  assert.strictEqual(typeof tcp1.triggerId, 'number', 'triggerId is a number');
  checkInvocations(tcp1, { init: 1 }, 'when calling server.listen');
}

// Calling net.connect creates another TCPWRAP synchronously
{
  net.connect(
    { port: server.address().port, host: server.address().address },
    common.mustCall(onconnected));
  const tcps = hooks.activitiesOfTypes('TCPWRAP');
  assert.strictEqual(
    tcps.length, 2,
    '2 TCPWRAPs present when client is connecting');
  process.nextTick(() => {
    const tcpconnects = hooks.activitiesOfTypes('TCPCONNECTWRAP');
    assert.strictEqual(
      tcpconnects.length, 1,
      '1 TCPCONNECTWRAP present when client is connecting');
  });

  tcp2 = tcps[1];
  assert.strictEqual(tcps.length, 2,
                     '2 TCPWRAP present when client is connecting');
  assert.strictEqual(tcp2.type, 'TCPWRAP', 'tcp wrap');
  assert.strictEqual(typeof tcp2.uid, 'number', 'uid is a number');
  assert.strictEqual(typeof tcp2.triggerId, 'number', 'triggerId is a number');

  checkInvocations(tcp1, { init: 1 }, 'tcp1 when client is connecting');
  checkInvocations(tcp2, { init: 1 }, 'tcp2 when client is connecting');
}

function onlistening() {
  assert.strictEqual(hooks.activitiesOfTypes('TCPWRAP').length, 2,
                     'two TCPWRAPs when server is listening');
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

  // only focusing on TCPCONNECTWRAP here
  const tcpconnects = hooks.activitiesOfTypes('TCPCONNECTWRAP');
  assert.strictEqual(
    tcpconnects.length, 1,
    'one TCPCONNECTWRAP present on tcp connection');
  tcpconnect = tcpconnects[0];
  assert.strictEqual(tcpconnect.type, 'TCPCONNECTWRAP', 'tcpconnect wrap');
  assert.strictEqual(typeof tcpconnect.uid, 'number', 'uid is a number');
  assert.strictEqual(typeof tcpconnect.triggerId,
                     'number', 'triggerId is a number');
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
  checkInvocations(tcp1, expected, 'tcp1 when client connects');
  checkInvocations(tcp2, { init: 1 }, 'tcp2 when client connects');
}

function onconnection(c) {
  serverConnected = true;
  ontcpConnection(true);

  const tcps = hooks.activitiesOfTypes([ 'TCPWRAP' ]);
  const tcpconnects = hooks.activitiesOfTypes('TCPCONNECTWRAP');
  assert.strictEqual(
    tcps.length, 3,
    '3 TCPWRAPs present when server receives connection');
  assert.strictEqual(
    tcpconnects.length, 1,
    'one TCPCONNECTWRAP present when server receives connection');
  tcp3 = tcps[2];
  assert.strictEqual(tcp3.type, 'TCPWRAP', 'tcp wrap');
  assert.strictEqual(typeof tcp3.uid, 'number', 'uid is a number');
  assert.strictEqual(typeof tcp3.triggerId, 'number', 'triggerId is a number');

  checkInvocations(tcp1, { init: 1, before: 1 },
                   'tcp1 when server receives connection');
  checkInvocations(tcp2, { init: 1 }, 'tcp2 when server receives connection');
  checkInvocations(tcp3, { init: 1 }, 'tcp3 when server receives connection');

  c.end();
  this.close(common.mustCall(onserverClosed));
}

function onserverClosed() {
  checkInvocations(tcp1, { init: 1, before: 1, after: 1, destroy: 1 },
                   'tcp1 when server is closed');
  checkInvocations(tcp2, { init: 1, before: 2, after: 2, destroy: 1 },
                   'tcp2 when server is closed');
  checkInvocations(tcp3, { init: 1, before: 1, after: 1 },
                   'tcp3 synchronously when server is closed');
  tick(2, () => {
    checkInvocations(tcp3, { init: 1, before: 2, after: 2, destroy: 1 },
                     'tcp3 when server is closed');
    checkInvocations(tcpconnect, { init: 1, before: 1, after: 1, destroy: 1 },
                     'tcpconnect when server is closed');
  });
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck([ 'TCPWRAP', 'TCPCONNECTWRAP' ]);

  checkInvocations(tcp1, { init: 1, before: 1, after: 1, destroy: 1 },
                   'tcp1 when process exits');
  checkInvocations(
    tcp2, { init: 1, before: 2, after: 2, destroy: 1 },
    'tcp2 when process exits');
  checkInvocations(
    tcp3, { init: 1, before: 2, after: 2, destroy: 1 },
    'tcp3 when process exits');
  checkInvocations(
    tcpconnect, { init: 1, before: 1, after: 1, destroy: 1 },
    'tcpconnect when process exits');
}
