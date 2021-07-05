'use strict';

// Modeled very closely on the AbortController implementation
// in https://github.com/mysticatea/abort-controller (MIT license)

const {
  ObjectAssign,
  ObjectDefineProperties,
  ObjectSetPrototypeOf,
  ObjectDefineProperty,
  Symbol,
  SymbolToStringTag,
  TypeError,
} = primordials;

const {
  defineEventHandler,
  EventTarget,
  Event,
  kTrustEvent
} = require('internal/event_target');
const {
  customInspectSymbol,
} = require('internal/util');
const { inspect } = require('internal/util/inspect');
const {
  codes: {
    ERR_INVALID_THIS,
  }
} = require('internal/errors');

const kAborted = Symbol('kAborted');

function customInspect(self, obj, depth, options) {
  if (depth < 0)
    return self;

  const opts = ObjectAssign({}, options, {
    depth: options.depth === null ? null : options.depth - 1
  });

  return `${self.constructor.name} ${inspect(obj, opts)}`;
}

function validateAbortSignal(obj) {
  if (obj?.[kAborted] === undefined)
    throw new ERR_INVALID_THIS('AbortSignal');
}

class AbortSignal extends EventTarget {
  constructor() {
    // eslint-disable-next-line no-restricted-syntax
    throw new TypeError('Illegal constructor');
  }

  get aborted() {
    validateAbortSignal(this);
    return !!this[kAborted];
  }

  [customInspectSymbol](depth, options) {
    return customInspect(this, {
      aborted: this.aborted
    }, depth, options);
  }

  static abort() {
    return createAbortSignal(true);
  }
}

ObjectDefineProperties(AbortSignal.prototype, {
  aborted: { enumerable: true }
});

ObjectDefineProperty(AbortSignal.prototype, SymbolToStringTag, {
  writable: false,
  enumerable: false,
  configurable: true,
  value: 'AbortSignal',
});

defineEventHandler(AbortSignal.prototype, 'abort');

function createAbortSignal(aborted = false) {
  const signal = new EventTarget();
  ObjectSetPrototypeOf(signal, AbortSignal.prototype);
  signal[kAborted] = aborted;
  return signal;
}

function abortSignal(signal) {
  if (signal[kAborted]) return;
  signal[kAborted] = true;
  const event = new Event('abort', {
    [kTrustEvent]: true
  });
  signal.dispatchEvent(event);
}

// TODO(joyeecheung): V8 snapshot does not support instance member
// initializers for now:
// https://bugs.chromium.org/p/v8/issues/detail?id=10704
const kSignal = Symbol('signal');

function validateAbortController(obj) {
  if (obj?.[kSignal] === undefined)
    throw new ERR_INVALID_THIS('AbortController');
}

class AbortController {
  constructor() {
    this[kSignal] = createAbortSignal();
  }

  get signal() {
    validateAbortController(this);
    return this[kSignal];
  }

  abort() {
    validateAbortController(this);
    abortSignal(this[kSignal]);
  }

  [customInspectSymbol](depth, options) {
    return customInspect(this, {
      signal: this.signal
    }, depth, options);
  }
}

ObjectDefineProperties(AbortController.prototype, {
  signal: { enumerable: true },
  abort: { enumerable: true }
});

ObjectDefineProperty(AbortController.prototype, SymbolToStringTag, {
  writable: false,
  enumerable: false,
  configurable: true,
  value: 'AbortController',
});

module.exports = {
  kAborted,
  AbortController,
  AbortSignal,
};
