'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('./tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const net = require('net');

common.refreshTmpDir();

const hooks = initHooks();
hooks.enable();
let pipe1, pipe2, pipe3;
let pipeconnect;

net.createServer(common.mustCall(function(c) {
  c.end();
  this.close();
  process.nextTick(maybeOnconnect.bind(null, 'server'));
})).listen(common.PIPE, common.mustCall(onlisten));

function onlisten() {
  let pipes = hooks.activitiesOfTypes('PIPEWRAP');
  let pipeconnects = hooks.activitiesOfTypes('PIPECONNECTWRAP');
  assert.strictEqual(pipes.length, 1);
  assert.strictEqual(pipeconnects.length, 0);

  net.connect(common.PIPE,
              common.mustCall(maybeOnconnect.bind(null, 'client')));

  pipes = hooks.activitiesOfTypes('PIPEWRAP');
  pipeconnects = hooks.activitiesOfTypes('PIPECONNECTWRAP');
  assert.strictEqual(pipes.length, 2);
  assert.strictEqual(pipeconnects.length, 1);

  pipe1 = pipes[0];
  pipe2 = pipes[1];
  pipeconnect = pipeconnects[0];

  assert.strictEqual(pipe1.type, 'PIPEWRAP');
  assert.strictEqual(pipe2.type, 'PIPEWRAP');
  assert.strictEqual(pipeconnect.type, 'PIPECONNECTWRAP');
  for (const a of [ pipe1, pipe2, pipeconnect ]) {
    assert.strictEqual(typeof a.uid, 'number');
    assert.strictEqual(typeof a.triggerAsyncId, 'number');
    checkInvocations(a, { init: 1 }, 'after net.connect');
  }
}

const awaitOnconnectCalls = new Set(['server', 'client']);
function maybeOnconnect(source) {
  // both server and client must call onconnect. On most OS's waiting for
  // the client is sufficient, but on CertOS 5 the sever needs to respond too.
  assert.ok(awaitOnconnectCalls.size > 0);
  awaitOnconnectCalls.delete(source);
  if (awaitOnconnectCalls.size > 0) return;

  const pipes = hooks.activitiesOfTypes('PIPEWRAP');
  const pipeconnects = hooks.activitiesOfTypes('PIPECONNECTWRAP');

  assert.strictEqual(pipes.length, 3);
  assert.strictEqual(pipeconnects.length, 1);
  pipe3 = pipes[2];
  assert.strictEqual(typeof pipe3.uid, 'number');
  assert.strictEqual(typeof pipe3.triggerAsyncId, 'number');

  checkInvocations(pipe1, { init: 1, before: 1, after: 1 },
                   'pipe1, client connected');
  checkInvocations(pipe2, { init: 1 }, 'pipe2, client connected');
  checkInvocations(pipeconnect, { init: 1, before: 1 },
                   'pipeconnect, client connected');
  checkInvocations(pipe3, { init: 1 }, 'pipe3, client connected');
  tick(5);
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('PIPEWRAP');
  hooks.sanityCheck('PIPECONNECTWRAP');
  // TODO(thlorenz) why have some of those 'before' and 'after' called twice
  checkInvocations(pipe1, { init: 1, before: 1, after: 1, destroy: 1 },
                   'pipe1, process exiting');
  checkInvocations(pipe2, { init: 1, before: 2, after: 2, destroy: 1 },
                   'pipe2, process exiting');
  checkInvocations(pipeconnect, { init: 1, before: 1, after: 1, destroy: 1 },
                   'pipeconnect, process exiting');
  checkInvocations(pipe3, { init: 1, before: 2, after: 2, destroy: 1 },
                   'pipe3, process exiting');
}
