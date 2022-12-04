'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('../common/tick');
const async_hooks = require('async_hooks');
const { AsyncResource } = async_hooks;

const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const hooks = initHooks();
hooks.enable();

assert.throws(
  () => new AsyncResource(), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });
assert.throws(() => {
  new AsyncResource('invalid_trigger_id', { triggerAsyncId: null });
}, {
  code: 'ERR_INVALID_ASYNC_ID',
  name: 'RangeError',
});

assert.strictEqual(
  new AsyncResource('default_trigger_id').triggerAsyncId(),
  async_hooks.executionAsyncId(),
);

// Create first custom event 'alcazares' with triggerAsyncId derived
// from async_hooks executionAsyncId
const alcaTriggerId = async_hooks.executionAsyncId();
const alcaEvent = new AsyncResource('alcazares', alcaTriggerId);
const alcazaresActivities = hooks.activitiesOfTypes([ 'alcazares' ]);

// Alcazares event was constructed and thus only has an `init` call
assert.strictEqual(alcazaresActivities.length, 1);
const alcazares = alcazaresActivities[0];
assert.strictEqual(alcazares.type, 'alcazares');
assert.strictEqual(typeof alcazares.uid, 'number');
assert.strictEqual(alcazares.triggerAsyncId, alcaTriggerId);
checkInvocations(alcazares, { init: 1 }, 'alcazares constructed');

assert.strictEqual(typeof alcaEvent.asyncId(), 'number');
assert.notStrictEqual(alcaEvent.asyncId(), alcaTriggerId);
assert.strictEqual(alcaEvent.triggerAsyncId(), alcaTriggerId);

alcaEvent.runInAsyncScope(() => {
  checkInvocations(alcazares, { init: 1, before: 1 },
                   'alcazares emitted before');
});
checkInvocations(alcazares, { init: 1, before: 1, after: 1 },
                 'alcazares emitted after');
alcaEvent.runInAsyncScope(() => {
  checkInvocations(alcazares, { init: 1, before: 2, after: 1 },
                   'alcazares emitted before again');
});
checkInvocations(alcazares, { init: 1, before: 2, after: 2 },
                 'alcazares emitted after again');
alcaEvent.emitDestroy();
tick(1, common.mustCall(tick1));

function tick1() {
  checkInvocations(alcazares, { init: 1, before: 2, after: 2, destroy: 1 },
                   'alcazares emitted destroy');

  // The below shows that we can pass any number as a trigger id
  const pobTriggerId = 111;
  const pobEvent = new AsyncResource('poblado', pobTriggerId);
  const pobladoActivities = hooks.activitiesOfTypes([ 'poblado' ]);
  const poblado = pobladoActivities[0];
  assert.strictEqual(poblado.type, 'poblado');
  assert.strictEqual(typeof poblado.uid, 'number');
  assert.strictEqual(poblado.triggerAsyncId, pobTriggerId);
  checkInvocations(poblado, { init: 1 }, 'poblado constructed');
  pobEvent.runInAsyncScope(() => {
    checkInvocations(poblado, { init: 1, before: 1 },
                     'poblado emitted before');
  });

  checkInvocations(poblado, { init: 1, before: 1, after: 1 },
                   'poblado emitted after');

  // After we disable the hooks we shouldn't receive any events anymore
  hooks.disable();
  alcaEvent.emitDestroy();
  tick(1, common.mustCall(tick2));

  function tick2() {
    checkInvocations(
      alcazares, { init: 1, before: 2, after: 2, destroy: 1 },
      'alcazares emitted destroy a second time after hooks disabled');
    pobEvent.emitDestroy();
    tick(1, common.mustCall(tick3));
  }

  function tick3() {
    checkInvocations(poblado, { init: 1, before: 1, after: 1 },
                     'poblado emitted destroy after hooks disabled');
  }
}
