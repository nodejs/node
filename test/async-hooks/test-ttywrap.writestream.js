'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('./tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');
const tty_fd = common.getTTYfd();

if (tty_fd < 0)
  return common.skip('no valid TTY fd available');
const ttyStream = (() => {
  try {
    return new (require('tty').WriteStream)(tty_fd);
  } catch (e) {
    return null;
  }
})();
if (ttyStream === null)
  return common.skip('no valid TTY fd available');

const hooks = initHooks();
hooks.enable();

const as = hooks.activitiesOfTypes('TTYWRAP');
assert.strictEqual(as.length, 1);
const tty = as[0];
assert.strictEqual(tty.type, 'TTYWRAP');
assert.strictEqual(typeof tty.uid, 'number');
assert.strictEqual(typeof tty.triggerId, 'number');
checkInvocations(tty, { init: 1 }, 'when tty created');

ttyStream
  .on('finish', common.mustCall(onfinish))
  .end(common.mustCall(onend));

checkInvocations(tty, { init: 1 }, 'when tty.end() was invoked ');

function onend() {
  tick(2, common.mustCall(() =>
    checkInvocations(
      tty, { init: 1, before: 1, after: 1, destroy: 1 },
      'when tty ended ')
  ));
}

function onfinish() {
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
