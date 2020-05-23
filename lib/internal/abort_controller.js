'use strict';

// Modeled very closely on the AbortController implementation
// in https://github.com/mysticatea/abort-controller (MIT license)

const {
  Object,
  Symbol,
} = primordials;

const {
  EventTarget,
  Event
} = require('internal/event_target');
const {
  customInspectSymbol,
  emitExperimentalWarning
} = require('internal/util');
const { inspect } = require('internal/util/inspect');

const kAborted = Symbol('kAborted');

function customInspect(self, obj, depth, options) {
  if (depth < 0)
    return self;

  const opts = Object.assign({}, options, {
    depth: options.depth === null ? null : options.depth - 1
  });

  return `${self.constructor.name} ${inspect(obj, opts)}`;
}

class AbortSignal extends EventTarget {
  get aborted() { return !!this[kAborted]; }

  [customInspectSymbol](depth, options) {
    return customInspect(this, {
      aborted: this.aborted
    }, depth, options);
  }
}

Object.defineProperties(AbortSignal.prototype, {
  aborted: { enumerable: true }
});

function abortSignal(signal) {
  if (signal[kAborted]) return;
  signal[kAborted] = true;
  const event = new Event('abort');
  if (typeof signal.onabort === 'function') {
    signal.onabort(event);
  }
  signal.dispatchEvent(event);
}

class AbortController {
  #signal = new AbortSignal();

  constructor() {
    emitExperimentalWarning('AbortController');
  }

  get signal() { return this.#signal; }
  abort() { abortSignal(this.#signal); }

  [customInspectSymbol](depth, options) {
    return customInspect(this, {
      signal: this.signal
    }, depth, options);
  }
}

Object.defineProperties(AbortController.prototype, {
  signal: { enumerable: true },
  abort: { enumerable: true }
});

module.exports = {
  AbortController,
  AbortSignal,
};
