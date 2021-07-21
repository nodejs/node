'use strict';

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypeSplice,
  ArrayPrototypePush,
  FunctionPrototypeBind
} = primordials;

const { setPromiseHooks } = internalBinding('async_wrap');
const { triggerUncaughtException } = internalBinding('errors');

const hooks = {
  init: [],
  before: [],
  after: [],
  resolve: []
};

function initAll(promise, parent) {
  for (const init of hooks.init.slice()) {
    try {
      init(promise, parent);
    } catch (err) {
      process.nextTick(() => {
        triggerUncaughtException(err, false);
      });
    }
  }
}

function makeRunHook(list) {
  return (promise) => {
    for (const hook of list.slice()) {
      try {
        hook(promise);
      } catch (err) {
        process.nextTick(() => {
          triggerUncaughtException(err, false);
        });
      }
    }
  };
}

const beforeAll = makeRunHook(hooks.before);
const afterAll = makeRunHook(hooks.after);
const resolveAll = makeRunHook(hooks.resolve);

function maybeFastPath(list, runAll) {
  return list.length > 1 ? runAll : list[0];
}

function update() {
  const init = maybeFastPath(hooks.init, initAll);
  const before = maybeFastPath(hooks.before, beforeAll);
  const after = maybeFastPath(hooks.after, afterAll);
  const resolve = maybeFastPath(hooks.resolve, resolveAll);
  setPromiseHooks(init, before, after, resolve);
}

function stop(list, hook) {
  const index = ArrayPrototypeIndexOf(list, hook);
  if (index >= 0) {
    ArrayPrototypeSplice(list, index, 1);
    update();
  }
}

function makeUseHook(list) {
  return (hook) => {
    ArrayPrototypePush(list, hook);
    update();
    return FunctionPrototypeBind(stop, null, list, hook);
  };
}

const onInit = makeUseHook(hooks.init);
const onBefore = makeUseHook(hooks.before);
const onAfter = makeUseHook(hooks.after);
const onResolve = makeUseHook(hooks.resolve);

function createHook({ init, before, after, resolve } = {}) {
  const hooks = [];

  if (init) ArrayPrototypePush(hooks, onInit(init));
  if (before) ArrayPrototypePush(hooks, onBefore(before));
  if (after) ArrayPrototypePush(hooks, onAfter(after));
  if (resolve) ArrayPrototypePush(hooks, onResolve(resolve));

  return () => {
    for (const stop of hooks) {
      stop();
    }
  };
}

module.exports = {
  createHook,
  onInit,
  onBefore,
  onAfter,
  onResolve
};
