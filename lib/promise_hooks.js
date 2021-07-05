'use strict';

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypeSplice,
  ArrayPrototypePush,
  FunctionPrototypeBind
} = primordials;

const { setPromiseHooks } = internalBinding('async_wrap');

const hooks = {
  init: [],
  before: [],
  after: [],
  resolve: []
};

function initAll(promise, parent) {
  for (const init of hooks.init) {
    init(promise, parent);
  }
}

function beforeAll(promise) {
  for (const before of hooks.before) {
    before(promise);
  }
}

function afterAll(promise) {
  for (const after of hooks.after) {
    after(promise);
  }
}

function resolveAll(promise) {
  for (const resolve of hooks.resolve) {
    resolve(promise);
  }
}

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
