'use strict';

const common = require('../common');
const assert = require('assert');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

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

// It is non-deterministic in which order onconnection and onconnected fire.
// Therefore we track here if we ended the connection already or not.
let endedConnection = false;
function onconnection(c) {
  assert.strictEqual(hooks.activitiesOfTypes('SHUTDOWNWRAP').length, 0);
  c.end();
  process.nextTick(() => {
    endedConnection = true;
    const as = hooks.activitiesOfTypes('SHUTDOWNWRAP');
    assert.strictEqual(as.length, 1);
    checkInvocations(as[0], { init: 1 }, 'after ending client connection');
    this.close(onserverClosed);
  });
}

function onconnected() {
  if (endedConnection) {
    assert.strictEqual(hooks.activitiesOfTypes('SHUTDOWNWRAP').length, 1);

  } else {
    assert.strictEqual(hooks.activitiesOfTypes('SHUTDOWNWRAP').length, 0);
  }
}

function onserverClosed() {
  const as = hooks.activitiesOfTypes('SHUTDOWNWRAP');
  checkInvocations(as[0], { init: 1, before: 1, after: 1, destroy: 1 },
                   'when server closed');
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('SHUTDOWNWRAP');
  const as = hooks.activitiesOfTypes('SHUTDOWNWRAP');
  const a = as[0];
  assert.strictEqual(a.type, 'SHUTDOWNWRAP');
  assert.strictEqual(typeof a.uid, 'number');
  assert.strictEqual(typeof a.triggerAsyncId, 'number');
  checkInvocations(as[0], { init: 1, before: 1, after: 1, destroy: 1 },
                   'when process exits');
}
