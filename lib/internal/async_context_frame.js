'use strict';

const {
  MapPrototypeForEach,
  MapPrototypeSet,
  SafeMap,
} = primordials;

const {
  getContinuationPreservedEmbedderData,
  setContinuationPreservedEmbedderData,
} = internalBinding('async_context_frame');

function copyEntry(value, key) {
  MapPrototypeSet(this, key, value);
}

class AsyncContextFrame extends SafeMap {
  constructor(store, data) {
    super();
    const previous = AsyncContextFrame.current();
    if (previous !== undefined) {
      // Copy without allocating iterator results and entry arrays.
      MapPrototypeForEach(previous, copyEntry, this);
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
