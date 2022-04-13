'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('../common/tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');
const dgram = require('dgram');

const hooks = initHooks();

hooks.enable();
const sock = dgram.createSocket('udp4');

const as = hooks.activitiesOfTypes('UDPWRAP');
const udpwrap = as[0];
assert.strictEqual(as.length, 1);
assert.strictEqual(udpwrap.type, 'UDPWRAP');
assert.strictEqual(typeof udpwrap.uid, 'number');
assert.strictEqual(typeof udpwrap.triggerAsyncId, 'number');
checkInvocations(udpwrap, { init: 1 }, 'after dgram.createSocket call');

sock.close(common.mustCall(onsockClosed));

function onsockClosed() {
  checkInvocations(udpwrap, { init: 1 }, 'when socket is closed');
  tick(2);
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('UDPWRAP');
  checkInvocations(udpwrap, { init: 1, destroy: 1 },
                   'when process exits');
}
