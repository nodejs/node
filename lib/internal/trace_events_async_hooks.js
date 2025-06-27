'use strict';

const {
  ObjectKeys,
  SafeMap,
  SafeSet,
  Symbol,
} = primordials;

const { trace } = internalBinding('trace_events');
const async_wrap = internalBinding('async_wrap');
const async_hooks = require('async_hooks');
const {
  CHAR_LOWERCASE_B,
  CHAR_LOWERCASE_E,
} = require('internal/constants');

// Use small letters such that chrome://tracing groups by the name.
// The behavior is not only useful but the same as the events emitted using
// the specific C++ macros.
const kBeforeEvent = CHAR_LOWERCASE_B;
const kEndEvent = CHAR_LOWERCASE_E;
const kTraceEventCategory = 'node,node.async_hooks';

const kEnabled = Symbol('enabled');

// It is faster to emit traceEvents directly from C++. Thus, this happens
// in async_wrap.cc. However, events emitted from the JavaScript API or the
// Embedder C++ API can't be emitted from async_wrap.cc. Thus they are
// emitted using the JavaScript API. To prevent emitting the same event
// twice the async_wrap.Providers list is used to filter the events.
const nativeProviders = new SafeSet(ObjectKeys(async_wrap.Providers));
const typeMemory = new SafeMap();

// Promises are not AsyncWrap resources so they should emit trace_events here.
nativeProviders.delete('PROMISE');

function createHook() {
  // In traceEvents it is not only the id but also the name that needs to be
  // repeated. Since async_hooks doesn't expose the provider type in the
  // non-init events, use a map to manually map the asyncId to the type name.

  const hook = async_hooks.createHook({
    init(asyncId, type, triggerAsyncId, resource) {
      if (nativeProviders.has(type)) return;

      typeMemory.set(asyncId, type);
      trace(kBeforeEvent, kTraceEventCategory,
            type, asyncId,
            {
              triggerAsyncId,
              executionAsyncId: async_hooks.executionAsyncId(),
            });
    },

    before(asyncId) {
      const type = typeMemory.get(asyncId);
      if (type === undefined) return;

      trace(kBeforeEvent, kTraceEventCategory, `${type}_CALLBACK`, asyncId);
    },

    after(asyncId) {
      const type = typeMemory.get(asyncId);
      if (type === undefined) return;

      trace(kEndEvent, kTraceEventCategory, `${type}_CALLBACK`, asyncId);
    },

    destroy(asyncId) {
      const type = typeMemory.get(asyncId);
      if (type === undefined) return;

      trace(kEndEvent, kTraceEventCategory, type, asyncId);

      // Cleanup asyncId to type map
      typeMemory.delete(asyncId);
    },
  });

  return {
    enable() {
      if (this[kEnabled])
        return;
      this[kEnabled] = true;
      hook.enable();
    },

    disable() {
      if (!this[kEnabled])
        return;
      this[kEnabled] = false;
      hook.disable();
      typeMemory.clear();
    },
  };
}

exports.createHook = createHook;
