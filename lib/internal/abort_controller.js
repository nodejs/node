'use strict';

// Modeled very closely on the AbortController implementation
// in https://github.com/mysticatea/abort-controller (MIT license)

const {
  ArrayPrototypePush,
  FunctionPrototypeCall,
  ObjectAssign,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectFreeze,
  ObjectSetPrototypeOf,
  PromiseResolve,
  PromiseWithResolvers,
  SafeFinalizationRegistry,
  SafeSet,
  SafeWeakRef,
  Symbol,
  SymbolToStringTag,
} = primordials;

const {
  defineEventHandler,
  EventTarget,
  Event,
  kEvents,
  kTrustEvent,
  kNewListener,
  kRemoveListener,
  kResistStopPropagation,
  kWeakHandler,
} = require('internal/event_target');
const { kMaxEventTargetListeners } = require('events');
const {
  customInspectSymbol,
  kEnumerableProperty,
} = require('internal/util');
const { inspect } = require('internal/util/inspect');
const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_THIS,
  },
  isErrorStackTraceLimitWritable,
} = require('internal/errors');
const {
  converters,
  createInterfaceConverter,
  createSequenceConverter,
} = require('internal/webidl');

const {
  validateObject,
  validateUint32,
  kValidateObjectAllowObjects,
} = require('internal/validators');

const {
  DOMException,
} = internalBinding('messaging');

const {
  clearTimeout,
  setTimeout,
} = require('timers');
const assert = require('internal/assert');

const {
  kDeserialize,
  kTransfer,
  kTransferList,
  markTransferMode,
} = require('internal/worker/js_transferable');

let _MessageChannel;

const kDontThrowSymbol = Symbol('kDontThrowSymbol');

const kTrustedEventInit = ObjectFreeze({ [kTrustEvent]: true });

// V8 doesn't optimize keyed super property access such as
// super[kNewListener](), so call the EventTarget implementations directly.
const {
  [kNewListener]: eventTargetNewListener,
  [kRemoveListener]: eventTargetRemoveListener,
} = EventTarget.prototype;

// Loading the MessageChannel and markTransferable have to be done lazily
// because otherwise we'll end up with a require cycle that ends up with
// an incomplete initialization of abort_controller.

function lazyMessageChannel() {
  _MessageChannel ??= require('internal/worker/io').MessageChannel;
  return new _MessageChannel();
}

const clearTimeoutRegistry = new SafeFinalizationRegistry((holder) => {
  if (holder.timeout !== undefined) {
    clearTimeout(holder.timeout);
  }
});

// Each followed composite signal is registered once. When it is collected,
// remove it from the dependant signals of its remaining sources.
const dependantSignalsCleanupRegistry = new SafeFinalizationRegistry(
  ({ sourceSignals, dependantSignalRef }) => {
    for (const sourceSignalRef of sourceSignals) {
      sourceSignalRef.deref()?.[kDependantSignals]?.delete(dependantSignalRef);
    }
  });

const gcPersistentSignals = new SafeSet();

// Each source signal is registered once, when it gets its first dependant.
// When it is collected, remove it from the source signals of its remaining
// dependants. Registering per source rather than per (source, dependant) pair
// avoids unregister tokens, which are expensive to register.
const sourceSignalsCleanupRegistry = new SafeFinalizationRegistry(({ sourceSignalRef, dependantSignals }) => {
  for (const composedSignalRef of dependantSignals) {
    const composedSignal = composedSignalRef.deref();
    if (composedSignal !== undefined) {
      composedSignal[kSourceSignals].delete(sourceSignalRef);

      if (composedSignal[kSourceSignals].size === 0) {
        // This signal will no longer abort. There's no need to keep it in the gcPersistentSignals set.
        gcPersistentSignals.delete(composedSignal);
      }
    }
  }
});

const kAborted = Symbol('kAborted');
const kReason = Symbol('kReason');
const kCloneData = Symbol('kCloneData');
const kTimeout = Symbol('kTimeout');
const kMakeTransferable = Symbol('kMakeTransferable');
const kComposite = Symbol('kComposite');
const kFollowing = Symbol('kFollowing');
const kWeakRef = Symbol('kWeakRef');
const kSourceSignals = Symbol('kSourceSignals');
const kDependantSignals = Symbol('kDependantSignals');

// A signal's own WeakRef, shared by every composite that follows it so that
// a long-lived source doesn't allocate a new WeakRef per AbortSignal.any().
function getWeakRef(signal) {
  return signal[kWeakRef] ??= new SafeWeakRef(signal);
}

function customInspect(self, obj, depth, options) {
  if (depth < 0)
    return self;

  const opts = ObjectAssign({}, options, {
    depth: options.depth === null ? null : options.depth - 1,
  });

  return `${self.constructor.name} ${inspect(obj, opts)}`;
}

function validateThisAbortSignal(obj) {
  if (obj?.[kAborted] === undefined)
    throw new ERR_INVALID_THIS('AbortSignal');
}

function refreshCompositeSignal(signal) {
  if (!signal[kComposite] || signal[kAborted] || !signal[kSourceSignals]?.size) {
    return;
  }

  for (const sourceSignalWeakRef of signal[kSourceSignals]) {
    const sourceSignal = sourceSignalWeakRef.deref();
    if (sourceSignal === undefined) {
      signal[kSourceSignals].delete(sourceSignalWeakRef);
      continue;
    }

    if (sourceSignal[kAborted]) {
      abortSignal(signal, sourceSignal[kReason]);
      return;
    }
  }
}

function followCompositeSignal(signal) {
  if (signal[kFollowing] || signal[kAborted] || !signal[kSourceSignals]?.size) {
    return;
  }

  const resultSignalWeakRef = getWeakRef(signal);
  const sourceSignals = signal[kSourceSignals];

  dependantSignalsCleanupRegistry.register(signal, {
    sourceSignals,
    dependantSignalRef: resultSignalWeakRef,
  });

  for (const sourceSignalWeakRef of sourceSignals) {
    const sourceSignal = sourceSignalWeakRef.deref();
    if (sourceSignal === undefined) {
      sourceSignals.delete(sourceSignalWeakRef);
      continue;
    }

    if (sourceSignal[kAborted]) {
      abortSignal(signal, sourceSignal[kReason]);
      return;
    }

    let dependantSignals = sourceSignal[kDependantSignals];
    if (dependantSignals === undefined) {
      dependantSignals = sourceSignal[kDependantSignals] = new SafeSet();
      sourceSignalsCleanupRegistry.register(sourceSignal, {
        sourceSignalRef: sourceSignalWeakRef,
        dependantSignals,
      });
    }
    dependantSignals.add(resultSignalWeakRef);
  }

  signal[kFollowing] = true;
}

// Because the AbortSignal timeout cannot be canceled, we don't want the
// presence of the timer alone to keep the AbortSignal from being garbage
// collected if it otherwise no longer accessible. We also don't want the
// timer to keep the Node.js process open on it's own. Therefore, we wrap
// the AbortSignal in a WeakRef and have the setTimeout callback close
// over the WeakRef rather than directly over the AbortSignal, and we unref
// the created timer object. Separately, we add the signal to a
// FinalizerRegistry that will clear the timeout when the signal is gc'd.
// The registry holds the timeout through a holder that is emptied once the
// timer fires, which releases the fired timer without having to unregister
// the signal. Registering with an unregister token is several times more
// expensive than registering without one.
function setWeakAbortSignalTimeout(weakRef, delay) {
  const holder = { timeout: undefined };
  holder.timeout = setTimeout(() => {
    holder.timeout = undefined;
    const signal = weakRef.deref();
    if (signal !== undefined) {
      gcPersistentSignals.delete(signal);
      abortSignal(
        signal,
        new DOMException(
          'The operation was aborted due to timeout',
          'TimeoutError'));
    }
  }, delay);
  holder.timeout.unref();
  return holder;
}

// The default reason captures no stack. A captured stack keeps the receivers
// and functions of the aborting caller's frames alive until it is formatted,
// so a retained signal would keep the aborted work alive, and formatting it
// eagerly costs several microseconds per abort. Callers that want the stack
// can pass their own reason.
// Refs: https://github.com/nodejs/node/issues/66192
function createDefaultAbortReason() {
  if (!isErrorStackTraceLimitWritable()) {
    return new DOMException('This operation was aborted', 'AbortError');
  }
  const limit = Error.stackTraceLimit;
  Error.stackTraceLimit = 0;
  const reason = new DOMException('This operation was aborted', 'AbortError');
  Error.stackTraceLimit = limit;
  return reason;
}

class AbortSignal extends EventTarget {
  #brand;

  static {
    converters.AbortSignal = createInterfaceConverter(
      'AbortSignal',
      (value) => typeof value === 'object' && value !== null && #brand in value,
    );
  }

  /**
   * Takes positional arguments rather than an options object: destructuring
   * the default kEmptyObject, which V8 keeps in dictionary mode, cost more
   * than the rest of the constructor.
   * @param {symbol | undefined} dontThrowSymbol
   * @param {boolean} [aborted]
   * @param {any} [reason]
   * @param {boolean} [transferable]
   * @param {boolean} [composite]
   * @private
   */
  constructor(dontThrowSymbol = undefined, aborted = false, reason = undefined,
              transferable = false, composite = false) {
    if (dontThrowSymbol !== kDontThrowSymbol) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
    super();

    this[kMaxEventTargetListeners] = 0;
    this[kAborted] = aborted;
    this[kReason] = reason;
    this[kComposite] = composite;
    // Signals that become sources of AbortSignal.any() get these slots later.
    // Initialize them up front so that adding them doesn't change the shape
    // of live signals and make the call sites that use them polymorphic.
    this[kWeakRef] = undefined;
    this[kDependantSignals] = undefined;
    if (composite) {
      this[kTimeout] = false;
      this[kFollowing] = false;
      this[kSourceSignals] = undefined;
    }
    if (transferable) {
      markTransferMode(this, false, true);
    }
  }

  /**
   * @type {boolean}
   */
  get aborted() {
    validateThisAbortSignal(this);
    refreshCompositeSignal(this);
    return !!this[kAborted];
  }

  /**
   * @type {any}
   */
  get reason() {
    validateThisAbortSignal(this);
    refreshCompositeSignal(this);
    return this[kReason];
  }

  throwIfAborted() {
    validateThisAbortSignal(this);
    refreshCompositeSignal(this);
    if (this[kAborted]) {
      throw this[kReason];
    }
  }

  [customInspectSymbol](depth, options) {
    return customInspect(this, {
      aborted: this.aborted,
    }, depth, options);
  }

  /**
   * @param {any} [reason]
   * @returns {AbortSignal}
   */
  static abort(reason = createDefaultAbortReason()) {
    return new AbortSignal(kDontThrowSymbol, true, reason);
  }

  /**
   * @param {number} delay
   * @returns {AbortSignal}
   */
  static timeout(delay) {
    validateUint32(delay, 'delay', false);
    const signal = new AbortSignal(kDontThrowSymbol);
    signal[kTimeout] = true;
    clearTimeoutRegistry.register(
      signal,
      setWeakAbortSignalTimeout(getWeakRef(signal), delay));
    return signal;
  }

  /**
   * @param {AbortSignal[]} signals
   * @returns {AbortSignal}
   */
  static any(signals) {
    const signalsArray = converters['sequence<AbortSignal>'](
      signals,
      { __proto__: null, context: 'signals' },
    );

    const resultSignal = new AbortSignal(kDontThrowSymbol, false, undefined, false, true);
    if (!signalsArray.length) {
      return resultSignal;
    }

    resultSignal[kSourceSignals] = new SafeSet();

    // Track if we have any timeout signals
    let hasTimeoutSignals = false;

    for (let i = 0; i < signalsArray.length; i++) {
      const signal = signalsArray[i];

      // Check if this is a timeout signal
      if (signal[kTimeout]) {
        hasTimeoutSignals = true;

        // Add the timeout signal to gcPersistentSignals to keep it alive
        // This is what the kNewListener method would do when adding abort listeners
        gcPersistentSignals.add(signal);
      }

      refreshCompositeSignal(signal);
      if (signal[kAborted]) {
        abortSignal(resultSignal, signal[kReason]);
        return resultSignal;
      }

      if (!signal[kComposite]) {
        resultSignal[kSourceSignals].add(getWeakRef(signal));
      } else if (!signal[kSourceSignals]) {
        continue;
      } else {
        for (const sourceSignalWeakRef of signal[kSourceSignals]) {
          const sourceSignal = sourceSignalWeakRef.deref();
          if (!sourceSignal) {
            continue;
          }
          assert(!sourceSignal[kComposite]);

          if (sourceSignal[kAborted]) {
            abortSignal(resultSignal, sourceSignal[kReason]);
            return resultSignal;
          }

          if (resultSignal[kSourceSignals].has(sourceSignalWeakRef)) {
            continue;
          }
          resultSignal[kSourceSignals].add(sourceSignalWeakRef);
        }
      }
    }

    if (hasTimeoutSignals && resultSignal[kSourceSignals].size > 0) {
      resultSignal[kTimeout] = true;
    }

    // Preserve abort state even if sources are collected before this signal is
    // observed. Following uses weak references so unused signals can be collected.
    followCompositeSignal(resultSignal);

    return resultSignal;
  }

  [kNewListener](size, type, listener, once, capture, passive, weak) {
    FunctionPrototypeCall(eventTargetNewListener, this, size, type, listener, once, capture, passive, weak);

    if (this[kComposite] && type === 'abort' && !this.aborted && size === 1) {
      followCompositeSignal(this);
    }

    const isTimeoutOrNonEmptyCompositeSignal = this[kTimeout] || (this[kComposite] && this[kSourceSignals]?.size);
    if (isTimeoutOrNonEmptyCompositeSignal &&
        type === 'abort' &&
        !this.aborted &&
        !weak &&
        size === 1) {
      // If this is a timeout signal, or a non-empty composite signal, and we're adding a non-weak abort
      // listener, then we don't want it to be gc'd while the listener
      // is attached and the timer still hasn't fired. So, we retain a
      // strong ref that is held for as long as the listener is registered.
      gcPersistentSignals.add(this);
    }
  }

  [kRemoveListener](size, type, listener, capture) {
    FunctionPrototypeCall(eventTargetRemoveListener, this, size, type, listener, capture);
    const isTimeoutOrNonEmptyCompositeSignal = this[kTimeout] || (this[kComposite] && this[kSourceSignals]?.size);
    if (isTimeoutOrNonEmptyCompositeSignal &&
        type === 'abort' &&
        size === 0 &&
        !this[kDependantSignals]?.size) {
      gcPersistentSignals.delete(this);
    }
  }

  [kTransfer]() {
    validateThisAbortSignal(this);
    const aborted = this.aborted;
    if (aborted) {
      const reason = this.reason;
      return {
        data: { aborted, reason },
        deserializeInfo: 'internal/abort_controller:ClonedAbortSignal',
      };
    }

    const { port1, port2 } = this[kCloneData];
    this[kCloneData] = undefined;

    this.addEventListener('abort', () => {
      port1.postMessage(this.reason);
      port1.close();
    }, { once: true });

    return {
      data: { port: port2 },
      deserializeInfo: 'internal/abort_controller:ClonedAbortSignal',
    };
  }

  [kTransferList]() {
    if (!this.aborted) {
      const { port1, port2 } = lazyMessageChannel();
      port1.unref();
      port2.unref();
      this[kCloneData] = {
        port1,
        port2,
      };
      return [port2];
    }
    return [];
  }

  [kDeserialize]({ aborted, reason, port }) {
    if (aborted) {
      this[kAborted] = aborted;
      this[kReason] = reason;
      return;
    }

    port.onmessage = ({ data }) => {
      abortSignal(this, data);
      port.close();
      port.onmessage = undefined;
    };
    // The receiving port, by itself, should never keep the event loop open.
    // The unref() has to be called *after* setting the onmessage handler.
    port.unref();
  }
}

converters['sequence<AbortSignal>'] = createSequenceConverter(converters.AbortSignal);

function ClonedAbortSignal() {
  return new AbortSignal(kDontThrowSymbol, false, undefined, true);
}
ClonedAbortSignal.prototype[kDeserialize] = () => {};

ObjectDefineProperties(AbortSignal.prototype, {
  aborted: kEnumerableProperty,
});

ObjectDefineProperty(AbortSignal.prototype, SymbolToStringTag, {
  __proto__: null,
  writable: false,
  enumerable: false,
  configurable: true,
  value: 'AbortSignal',
});

defineEventHandler(AbortSignal.prototype, 'abort');

// https://dom.spec.whatwg.org/#dom-abortsignal-abort
function abortSignal(signal, reason) {
  // 1. If signal is aborted, then return.
  if (signal[kAborted]) return;

  // 2. Set signal's abort reason to reason if it is given;
  //    otherwise to a new "AbortError" DOMException.
  signal[kAborted] = true;
  signal[kReason] = reason;

  // 3. Let dependentSignalsToAbort be a new list.
  // The list is only allocated when there are dependent signals.
  let dependentSignalsToAbort;

  // 4. For each dependentSignal of signal's dependent signals:
  const dependantSignals = signal[kDependantSignals];
  if (dependantSignals !== undefined && dependantSignals.size > 0) {
    dependentSignalsToAbort = ObjectSetPrototypeOf([], null);
    dependantSignals.forEach((s) => {
      const dependentSignal = s.deref();
      // 1. If dependentSignal is not aborted, then:
      if (dependentSignal && !dependentSignal[kAborted]) {
        // 1. Set dependentSignal's abort reason to signal's abort reason.
        dependentSignal[kReason] = reason;
        dependentSignal[kAborted] = true;
        // 2. Append dependentSignal to dependentSignalsToAbort.
        ArrayPrototypePush(dependentSignalsToAbort, dependentSignal);
      }
    });
  }

  // 5. Run the abort steps for signal
  runAbort(signal);

  // 6. For each dependentSignal of dependentSignalsToAbort,
  //    run the abort steps for dependentSignal.
  if (dependentSignalsToAbort !== undefined) {
    for (let i = 0; i < dependentSignalsToAbort.length; i++) {
      const dependentSignal = dependentSignalsToAbort[i];
      runAbort(dependentSignal);
      // A transitively-aborted dependent signal can never abort again, so there
      // is no longer any reason to keep it alive. Dropping it from
      // gcPersistentSignals lets it be collected, which in turn prunes its
      // WeakRef from its sources' kDependantSignals sets. Otherwise, an observed
      // composite that follows a long-lived source stays retained forever and
      // its entry accumulates in that source's kDependantSignals.
      gcPersistentSignals.delete(dependentSignal);
    }
  }

  // Clean up the signal from gcPersistentSignals
  gcPersistentSignals.delete(signal);

  // If this is a composite signal, also remove all of its source signals from gcPersistentSignals
  // when they get dereferenced from the signal's kSourceSignals set
  if (signal[kComposite] && signal[kSourceSignals]) {
    signal[kSourceSignals].forEach((sourceWeakRef) => {
      const sourceSignal = sourceWeakRef.deref();
      if (sourceSignal) {
        gcPersistentSignals.delete(sourceSignal);
      }
    });
  }
}

// To run the abort steps for an AbortSignal signal
function runAbort(signal) {
  // Firing an event at a signal without abort listeners has no observable
  // effect, so skip allocating and dispatching it.
  const root = signal[kEvents]?.get('abort');
  if (root === undefined || root.next === undefined) {
    return;
  }
  const event = new Event('abort', kTrustedEventInit);
  signal.dispatchEvent(event);
}

class AbortController {
  #signal;

  /**
   * @type {AbortSignal}
   */
  get signal() {
    this.#signal ??= new AbortSignal(kDontThrowSymbol);

    return this.#signal;
  }

  /**
   * @param {any} [reason]
   */
  abort(reason = createDefaultAbortReason()) {
    abortSignal(this.#signal ??= new AbortSignal(kDontThrowSymbol), reason);
  }

  [customInspectSymbol](depth, options) {
    return customInspect(this, {
      signal: this.signal,
    }, depth, options);
  }

  static [kMakeTransferable]() {
    const controller = new AbortController();
    controller.#signal = new AbortSignal(kDontThrowSymbol, false, undefined, true);
    return controller;
  }
}

/**
 * Enables the AbortSignal to be transferable using structuredClone/postMessage.
 * @param {AbortSignal} signal
 * @returns {AbortSignal}
 */
function transferableAbortSignal(signal) {
  if (signal?.[kAborted] === undefined)
    throw new ERR_INVALID_ARG_TYPE('signal', 'AbortSignal', signal);
  markTransferMode(signal, false, true);
  return signal;
}

/**
 * Creates an AbortController with a transferable AbortSignal
 * @returns {AbortController}
 */
function transferableAbortController() {
  return AbortController[kMakeTransferable]();
}

/**
 * @param {AbortSignal} signal
 * @param {any} resource
 * @returns {Promise<void>}
 */
async function aborted(signal, resource) {
  converters.AbortSignal(signal, { __proto__: null, context: 'signal' });
  validateObject(resource, 'resource', kValidateObjectAllowObjects);
  if (signal.aborted)
    return PromiseResolve();
  const abortPromise = PromiseWithResolvers();
  const opts = { __proto__: null, [kWeakHandler]: resource, once: true, [kResistStopPropagation]: true };
  signal.addEventListener('abort', abortPromise.resolve, opts);
  return abortPromise.promise;
}

ObjectDefineProperties(AbortController.prototype, {
  signal: kEnumerableProperty,
  abort: kEnumerableProperty,
});

ObjectDefineProperty(AbortController.prototype, SymbolToStringTag, {
  __proto__: null,
  writable: false,
  enumerable: false,
  configurable: true,
  value: 'AbortController',
});

module.exports = {
  AbortController,
  AbortSignal,
  ClonedAbortSignal,
  abortSignal,
  aborted,
  transferableAbortSignal,
  transferableAbortController,
};
