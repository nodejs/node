'use strict'

const binding = process.binding('async_wrap');
const promise = require('internal/promise');
const assert = require('assert');
var installedPromises = false;
var currentHooks = null;

module.exports = {
  Providers: binding.Providers,
  setupHooks,
  disable,
  enable
};

function setupHooks (hooks) {
  currentHooks = Object.assign({
    init: noop,
    pre: noop,
    post: noop,
    destroy: noop
  }, hooks);
  assert(typeof currentHooks.init === 'function');
  assert(typeof currentHooks.pre === 'function');
  assert(typeof currentHooks.post === 'function');
  assert(typeof currentHooks.destroy === 'function');

  return binding.setupHooks(
    hooks.init,
    hooks.pre,
    hooks.post,
    hooks.destroy
  );
}

function disable () {
  currentHooks = null;
  return binding.disable();
}

function enable () {
  if (!installedPromises) {
    _installPromises();
  }
  return binding.enable();
}

function _installPromises () {
  promise.addAsyncHooks({
    init (promise) {
      if (!currentHooks) {
        return;
      }
      currentHooks.init.call(this, binding.Providers.PROMISE)
    },
    wrap (promise, fn, ctxt) {
      return function () {
        try {
          if (currentHooks) {
            currentHooks.pre.call(ctxt.promise);
          }
        } finally {
        }
        try {
          return fn.apply(this, arguments);
        } finally {
          if (currentHooks) {
            try {
              currentHooks.post.call(ctxt.promise);
            } finally {
            }
            try {
              currentHooks.destroy.call(ctxt.promise);
            } finally {
            }
          }
        }
      }
    }
  })
}

function noop () {
}
