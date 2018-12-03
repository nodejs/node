'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('../common/tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const net = require('net');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const hooks = initHooks();
hooks.enable();
let pipe1, pipe2;
let pipeserver;
let pipeconnect;

net.createServer(common.mustCall(function(c) {
  c.end();
  this.close();
  process.nextTick(maybeOnconnect.bind(null, 'server'));
})).listen(common.PIPE, common.mustCall(onlisten));

function onlisten() {
  const pipeservers = hooks.activitiesOfTypes('PIPESERVERWRAP');
  let pipeconnects = hooks.activitiesOfTypes('PIPECONNECTWRAP');
  assert.strictEqual(pipeservers.length, 1);
  assert.strictEqual(pipeconnects.length, 0);

  net.connect(common.PIPE,
              common.mustCall(maybeOnconnect.bind(null, 'client')));

  const pipes = hooks.activitiesOfTypes('PIPEWRAP');
  pipeconnects = hooks.activitiesOfTypes('PIPECONNECTWRAP');
  assert.strictEqual(pipes.length, 1);
  assert.strictEqual(pipeconnects.length, 1);

  pipeserver = pipeservers[0];
  pipe1 = pipes[0];
  pipeconnect = pipeconnects[0];

  assert.strictEqual(pipeserver.type, 'PIPESERVERWRAP');
  assert.strictEqual(pipe1.type, 'PIPEWRAP');
  assert.strictEqual(pipeconnect.type, 'PIPECONNECTWRAP');
  for (const a of [ pipeserver, pipe1, pipeconnect ]) {
    assert.strictEqual(typeof a.uid, 'number');
    assert.strictEqual(typeof a.triggerAsyncId, 'number');
    checkInvocations(a, { init: 1 }, 'after net.connect');
  }
}

const awaitOnconnectCalls = new Set(['server', 'client']);
function maybeOnconnect(source) {
  // Both server and client must call onconnect. On most OS's waiting for
  // the client is sufficient, but on CentOS 5 the sever needs to respond too.
  assert.ok(awaitOnconnectCalls.size > 0);
  awaitOnconnectCalls.delete(source);
  if (awaitOnconnectCalls.size > 0) return;

  const pipes = hooks.activitiesOfTypes('PIPEWRAP');
  const pipeconnects = hooks.activitiesOfTypes('PIPECONNECTWRAP');

  assert.strictEqual(pipes.length, 2);
  assert.strictEqual(pipeconnects.length, 1);
  pipe2 = pipes[1];
  assert.strictEqual(typeof pipe2.uid, 'number');
  assert.strictEqual(typeof pipe2.triggerAsyncId, 'number');

  checkInvocations(pipeserver, { init: 1, before: 1, after: 1 },
                   'pipeserver, client connected');
  checkInvocations(pipe1, { init: 1 }, 'pipe1, client connected');
  checkInvocations(pipeconnect, { init: 1, before: 1 },
                   'pipeconnect, client connected');
  checkInvocations(pipe2, { init: 1 }, 'pipe2, client connected');
  tick(5);
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('PIPEWRAP');
  hooks.sanityCheck('PIPESERVERWRAP');
  hooks.sanityCheck('PIPECONNECTWRAP');
  // TODO(thlorenz) why have some of those 'before' and 'after' called twice
  checkInvocations(pipeserver, { init: 1, before: 1, after: 1, destroy: 1 },
                   'pipeserver, process exiting');
  checkInvocations(pipe1, { init: 1, before: 2, after: 2, destroy: 1 },
                   'pipe1, process exiting');
  checkInvocations(pipeconnect, { init: 1, before: 1, after: 1, destroy: 1 },
                   'pipeconnect, process exiting');
  checkInvocations(pipe2, { init: 1, before: 2, after: 2, destroy: 1 },
                   'pipe2, process exiting');
}
