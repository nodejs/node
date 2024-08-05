'use strict';

const {
  getContinuationPreservedEmbedderData,
  setContinuationPreservedEmbedderData,
} = internalBinding('async_context_frame');

let enabled_;

class AsyncContextFrame extends Map {
  constructor(store, data) {
    super(AsyncContextFrame.current());
    this.set(store, data);
  }

  static get enabled() {
    enabled_ ??= require('internal/options')
      .getOptionValue('--experimental-async-context-frame');
    return enabled_;
  }

  static current() {
    if (this.enabled) {
      return getContinuationPreservedEmbedderData();
    }
  }

  static set(frame) {
    if (this.enabled) {
      setContinuationPreservedEmbedderData(frame);
    }
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
