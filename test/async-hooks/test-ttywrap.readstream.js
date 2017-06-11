'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('./tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const hooks = initHooks();
hooks.enable();

const ReadStream = require('tty').ReadStream;
const ttyStream = new ReadStream(0);

const as = hooks.activitiesOfTypes('TTYWRAP');
assert.strictEqual(as.length, 1);
const tty = as[0];
assert.strictEqual(tty.type, 'TTYWRAP');
assert.strictEqual(typeof tty.uid, 'number');
assert.strictEqual(typeof tty.triggerId, 'number');
checkInvocations(tty, { init: 1 }, 'when tty created');

ttyStream.end(common.mustCall(onend));

checkInvocations(tty, { init: 1 }, 'when tty.end() was invoked ');

function onend() {
  tick(2, common.mustCall(() =>
    checkInvocations(
      tty, { init: 1, before: 1, after: 1, destroy: 1 },
      'when tty ended ')
  ));
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('TTYWRAP');
  checkInvocations(tty, { init: 1, before: 1, after: 1, destroy: 1 },
                   'when process exits');
}
