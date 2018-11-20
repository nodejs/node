'use strict';

const common = require('../common');
const assert = require('assert');

// general hook test setup
const tick = require('./tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const hooks = initHooks();
hooks.enable();

if (!process.stdout.isTTY)
  return common.skip('no valid writable TTY available');

// test specific setup
const checkInitOpts = { init: 1 };
const checkEndedOpts = { init: 1, before: 1, after: 1, destroy: 1 };

// test code
//
// listen to stdin except on Windows
const activities = hooks.activitiesOfTypes('TTYWRAP');
assert.strictEqual(activities.length, 1);

const tty = activities[0];
assert.strictEqual(tty.type, 'TTYWRAP');
assert.strictEqual(typeof tty.uid, 'number');
assert.strictEqual(typeof tty.triggerAsyncId, 'number');
checkInvocations(tty, checkInitOpts, 'when tty created');

const delayedOnCloseHandler = common.mustCall(() => {
  checkInvocations(tty, checkEndedOpts, 'when tty ended');
});
process.stdout.on('error', (err) => assert.fail(err));
process.stdout.on('close', common.mustCall(() =>
  tick(2, delayedOnCloseHandler)
));
process.stdout.destroy();
checkInvocations(tty, checkInitOpts, 'when tty.end() was invoked');

process.on('exit', () => {
  hooks.disable();
  hooks.sanityCheck('TTYWRAP');
  checkInvocations(tty, checkEndedOpts, 'when process exits');
});
