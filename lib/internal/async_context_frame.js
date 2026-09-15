'use strict';

const {
  SafeMap,
} = primordials;

const {
  getContinuationPreservedEmbedderData,
  setContinuationPreservedEmbedderData,
} = internalBinding('async_context_frame');

function activeCurrent() {
  return getContinuationPreservedEmbedderData();
}

function activeSet(frame) {
  setContinuationPreservedEmbedderData(frame);
}

function activeExchange(frame) {
  const prior = getContinuationPreservedEmbedderData();
  setContinuationPreservedEmbedderData(frame);
  return prior;
}

function activeDisable(store) {
  getContinuationPreservedEmbedderData()?.disable(store);
}

class InactiveAsyncContextFrame extends SafeMap {
  static get enabled() {
    enabled_ ??= checkEnabled();
    return enabled_;
  }

  static current() {}
  static set(frame) {}
  static exchange(frame) {}
  static disable(store) {}
}

class AsyncContextFrame extends InactiveAsyncContextFrame {
  constructor(store, data) {
    super(AsyncContextFrame.current());
    this.set(store, data);
  }

  disable(store) {
    this.delete(store);
  }
}

let enabled_;

function checkEnabled() {
  const enabled = require('internal/options')
    .getOptionValue('--async-context-frame');

  // If enabled, install the active implementations directly. We use props
  // rather than a prototype replacement to preserve V8 optimizations.
  if (enabled) {
    AsyncContextFrame.current = activeCurrent;
    AsyncContextFrame.set = activeSet;
    AsyncContextFrame.exchange = activeExchange;
    AsyncContextFrame.disable = activeDisable;
  }

  return enabled;
}

module.exports = AsyncContextFrame;
