'use strict';

const {
  SafeMap,
} = primordials;

const {
  getContinuationPreservedEmbedderData,
  setContinuationPreservedEmbedderData,
} = internalBinding('async_context_frame');

function copyEntry(value, key) {
  this.set(key, value);
}

class AsyncContextFrame extends SafeMap {
  constructor(store, data) {
    super();
    // Avoid the iterator protocol.
    const prior = AsyncContextFrame.current();
    if (prior !== undefined) {
      prior.forEach(copyEntry, this);
    }
    this.set(store, data);
  }

  static current() {
    return getContinuationPreservedEmbedderData();
  }

  static set(frame) {
    setContinuationPreservedEmbedderData(frame);
  }

  static exchange(frame) {
    const prior = this.current();
    this.set(frame);
    return prior;
  }

  static disable(store) {
    const frame = this.current();
    frame?.disable(store);
  }

  disable(store) {
    this.delete(store);
  }
}

module.exports = AsyncContextFrame;
