'use strict';

const {
  ArrayPrototypeAt,
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypePushApply,
  ArrayPrototypeSlice,
  ArrayPrototypeSplice,
  FunctionPrototypeCall,
  ObjectDefineProperty,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  PromisePrototypeThen,
  PromiseReject,
  ReflectApply,
  SafeFinalizationRegistry,
  SafeMap,
  SymbolAsyncIterator,
  SymbolDispose,
  SymbolHasInstance,
  SymbolIterator,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');
const {
  validateFunction,
} = require('internal/validators');

const { triggerUncaughtException } = internalBinding('errors');

const dc_binding = internalBinding('diagnostics_channel');
const { subscribers: subscriberCounts } = dc_binding;

const { WeakReference } = require('internal/util');
const { isPromise } = require('internal/util/types');

// Can't delete when weakref count reaches 0 as it could increment again.
// Only GC can be used as a valid time to clean up the channels map.
class WeakRefMap extends SafeMap {
  #finalizers = new SafeFinalizationRegistry((key) => {
    // Check that the key doesn't have any value before deleting, as the WeakRef for the key
    // may have been replaced since finalization callbacks aren't synchronous with GC.
    if (!this.has(key)) this.delete(key);
  });

  set(key, value) {
    this.#finalizers.register(value, key);
    return super.set(key, new WeakReference(value));
  }

  get(key) {
    return super.get(key)?.get();
  }

  has(key) {
    return !!this.get(key);
  }

  incRef(key) {
    return super.get(key)?.incRef();
  }

  decRef(key) {
    return super.get(key)?.decRef();
  }
}

function markActive(channel) {
  // eslint-disable-next-line no-use-before-define
  ObjectSetPrototypeOf(channel, ActiveChannel.prototype);
  channel._subscribers = [];
  channel._stores = new SafeMap();
}

function maybeMarkInactive(channel) {
  // When there are no more active subscribers or bound, restore to fast prototype.
  if (!channel._subscribers.length && !channel._stores.size) {
    // eslint-disable-next-line no-use-before-define
    ObjectSetPrototypeOf(channel, Channel.prototype);
    channel._subscribers = undefined;
    channel._stores = undefined;
  }
}

class RunStoresScope {
  #stack;

  constructor(activeChannel, data) {
    // eslint-disable-next-line no-restricted-globals
    using stack = new DisposableStack();

    // Enter stores using withScope
    if (activeChannel._stores) {
      for (const entry of activeChannel._stores.entries()) {
        const store = entry[0];
        const transform = entry[1];

        let newContext = data;
        if (transform) {
          try {
            newContext = transform(data);
          } catch (err) {
            process.nextTick(() => {
              triggerUncaughtException(err, false);
            });
            continue;
          }
        }

        stack.use(store.withScope(newContext));
      }
    }

    // Publish data
    activeChannel.publish(data);

    // Transfer ownership of the stack
    this.#stack = stack.move();
  }

  [SymbolDispose]() {
    this.#stack[SymbolDispose]();
  }
}

// TODO(qard): should there be a C++ channel interface?
class ActiveChannel {
  subscribe(subscription) {
    validateFunction(subscription, 'subscription');
    this._subscribers = ArrayPrototypeSlice(this._subscribers);
    ArrayPrototypePush(this._subscribers, subscription);
    channels.incRef(this.name);
    if (this._index !== undefined) subscriberCounts[this._index]++;
  }

  unsubscribe(subscription) {
    const index = ArrayPrototypeIndexOf(this._subscribers, subscription);
    if (index === -1) return false;

    const before = ArrayPrototypeSlice(this._subscribers, 0, index);
    const after = ArrayPrototypeSlice(this._subscribers, index + 1);
    this._subscribers = before;
    ArrayPrototypePushApply(this._subscribers, after);

    channels.decRef(this.name);
    if (this._index !== undefined) subscriberCounts[this._index]--;
    maybeMarkInactive(this);

    return true;
  }

  bindStore(store, transform) {
    const replacing = this._stores.has(store);
    if (!replacing) {
      channels.incRef(this.name);
      if (this._index !== undefined) subscriberCounts[this._index]++;
    }
    this._stores.set(store, transform);
  }

  unbindStore(store) {
    if (!this._stores.has(store)) {
      return false;
    }

    this._stores.delete(store);

    channels.decRef(this.name);
    if (this._index !== undefined) subscriberCounts[this._index]--;
    maybeMarkInactive(this);

    return true;
  }

  get hasSubscribers() {
    return true;
  }

  publish(data) {
    const subscribers = this._subscribers;
    for (let i = 0; i < (subscribers?.length || 0); i++) {
      try {
        const onMessage = subscribers[i];
        onMessage(data, this.name);
      } catch (err) {
        process.nextTick(() => {
          triggerUncaughtException(err, false);
        });
      }
    }
  }

  withStoreScope(data) {
    return new RunStoresScope(this, data);
  }

  runStores(data, fn, thisArg, ...args) {
    // eslint-disable-next-line no-unused-vars
    using scope = this.withStoreScope(data);
    return ReflectApply(fn, thisArg, args);
  }
}

class Channel {
  constructor(name) {
    this._subscribers = undefined;
    this._stores = undefined;
    this.name = name;
    if (typeof name === 'string') {
      this._index = dc_binding.getOrCreateChannelIndex(name);
    }

    channels.set(name, this);
  }

  static [SymbolHasInstance](instance) {
    const prototype = ObjectGetPrototypeOf(instance);
    return prototype === Channel.prototype ||
           prototype === ActiveChannel.prototype;
  }

  subscribe(subscription) {
    markActive(this);
    this.subscribe(subscription);
  }

  unsubscribe() {
    return false;
  }

  bindStore(store, transform) {
    markActive(this);
    this.bindStore(store, transform);
  }

  unbindStore() {
    return false;
  }

  get hasSubscribers() {
    return false;
  }

  publish() {}

  runStores(data, fn, thisArg, ...args) {
    return ReflectApply(fn, thisArg, args);
  }

  withStoreScope() {
    // Return no-op disposable for inactive channels
    return {
      [SymbolDispose]() {},
    };
  }
}

const channels = new WeakRefMap();

function channel(name) {
  const channel = channels.get(name);
  if (channel) return channel;

  if (typeof name !== 'string' && typeof name !== 'symbol') {
    throw new ERR_INVALID_ARG_TYPE('channel', ['string', 'symbol'], name);
  }

  return new Channel(name);
}

function subscribe(name, subscription) {
  return channel(name).subscribe(subscription);
}

function unsubscribe(name, subscription) {
  return channel(name).unsubscribe(subscription);
}

function hasSubscribers(name) {
  const channel = channels.get(name);
  if (!channel) return false;

  return channel.hasSubscribers;
}

const boundedEvents = [
  'start',
  'end',
];

const traceEvents = [
  'start',
  'end',
  'asyncStart',
  'asyncEnd',
  'error',
];
const asyncTraceEvents = [
  'asyncStart',
  'asyncEnd',
  'error',
];
const iteratorTraceEvents = [
  'yield',
  'return',
  'error',
];

function assertChannel(value, name) {
  if (!(value instanceof Channel)) {
    throw new ERR_INVALID_ARG_TYPE(name, ['Channel'], value);
  }
}

function isObjectLike(value) {
  return typeof value === 'object' && value !== null;
}

function emitNonThenableWarning(fn) {
  process.emitWarning(`tracePromise was called with the function '${fn.name || '<anonymous>'}', ` +
                      'which returned a non-thenable.');
}

function channelFromMap(nameOrChannels, name, className) {
  if (typeof nameOrChannels === 'string') {
    return channel(`tracing:${nameOrChannels}:${name}`);
  }

  if (typeof nameOrChannels === 'object' && nameOrChannels !== null) {
    const channel = nameOrChannels[name];
    assertChannel(channel, `nameOrChannels.${name}`);
    return channel;
  }

  throw new ERR_INVALID_ARG_TYPE('nameOrChannels',
                                 ['string', 'object', className],
                                 nameOrChannels);
}

function defineTracingChannels(target, nameOrChannels, eventNames) {
  for (let i = 0; i < eventNames.length; ++i) {
    const eventName = eventNames[i];
    ObjectDefineProperty(target, eventName, {
      __proto__: null,
      value: channelFromMap(nameOrChannels, eventName, 'TracingChannel'),
    });
  }
}

class BoundedChannelScope {
  #context;
  #end;
  #scope;

  constructor(boundedChannel, context) {
    // Only proceed if there are subscribers
    if (!boundedChannel.hasSubscribers) {
      return;
    }

    const { start, end } = boundedChannel;
    this.#context = context;
    this.#end = end;

    // Use RunStoresScope for the start channel
    this.#scope = new RunStoresScope(start, context);
  }

  [SymbolDispose]() {
    if (!this.#scope) {
      return;
    }

    // Publish end event
    this.#end.publish(this.#context);

    // Dispose the start scope to restore stores
    this.#scope[SymbolDispose]();
    this.#scope = undefined;
  }
}

class BoundedChannel {
  constructor(nameOrChannels) {
    for (let i = 0; i < boundedEvents.length; ++i) {
      const eventName = boundedEvents[i];
      ObjectDefineProperty(this, eventName, {
        __proto__: null,
        value: channelFromMap(nameOrChannels, eventName, 'BoundedChannel'),
      });
    }
  }

  get hasSubscribers() {
    return this.start?.hasSubscribers ||
      this.end?.hasSubscribers;
  }

  subscribe(handlers) {
    for (let i = 0; i < boundedEvents.length; ++i) {
      const name = boundedEvents[i];
      if (!handlers[name]) continue;

      this[name]?.subscribe(handlers[name]);
    }
  }

  unsubscribe(handlers) {
    let done = true;

    for (let i = 0; i < boundedEvents.length; ++i) {
      const name = boundedEvents[i];
      if (!handlers[name]) continue;

      if (!this[name]?.unsubscribe(handlers[name])) {
        done = false;
      }
    }

    return done;
  }

  withScope(context = {}) {
    return new BoundedChannelScope(this, context);
  }

  run(context, fn, thisArg, ...args) {
    context ??= {};
    // eslint-disable-next-line no-unused-vars
    using scope = this.withScope(context);
    return ReflectApply(fn, thisArg, args);
  }
}

function boundedChannel(nameOrChannels) {
  return new BoundedChannel(nameOrChannels);
}

function normalizeContext(context, thisArg, args) {
  if (context === undefined) {
    return { __proto__: null };
  }

  if (typeof context === 'function') {
    context = ReflectApply(context, thisArg, args);
    if (context === undefined) {
      return { __proto__: null };
    }
  }

  if (isObjectLike(context)) {
    return context;
  }

  throw new ERR_INVALID_ARG_TYPE('options.context', ['Function', 'Object'], context);
}

function getContextSection(context, name) {
  if (!isObjectLike(context[name])) {
    context[name] = { __proto__: null };
  }

  return context[name];
}

function normalizeCallbackOutcome(args, mapOutcome, thisArg) {
  if (mapOutcome) {
    return ReflectApply(mapOutcome, thisArg, args);
  }

  const error = args[0];
  if (error != null) {
    return { __proto__: null, error };
  }

  return { __proto__: null, result: args[1] };
}

function applyCallbackOutcome(callbackContext, outcome) {
  if (!isObjectLike(outcome)) {
    return;
  }

  if ('error' in outcome) {
    callbackContext.error = outcome.error;
  }

  if ('result' in outcome) {
    callbackContext.result = outcome.result;
  }
}

function tracingChannelHasSubscribers(target, eventNames) {
  for (let i = 0; i < eventNames.length; ++i) {
    if (target[eventNames[i]]?.hasSubscribers) {
      return true;
    }
  }

  return false;
}

function tracingChannelSubscribe(target, handlers, eventNames) {
  for (let i = 0; i < eventNames.length; ++i) {
    const name = eventNames[i];
    if (!handlers[name]) continue;

    target[name]?.subscribe(handlers[name]);
  }
}

function tracingChannelUnsubscribe(target, handlers, eventNames) {
  let done = true;

  for (let i = 0; i < eventNames.length; ++i) {
    const name = eventNames[i];
    if (!handlers[name]) continue;

    if (!target[name]?.unsubscribe(handlers[name])) {
      done = false;
    }
  }

  return done;
}

class SyncTracingChannel extends BoundedChannel {
  constructor(nameOrChannels) {
    if (typeof nameOrChannels === 'string') {
      super(nameOrChannels);
    } else {
      super({ start: nameOrChannels.start, end: nameOrChannels.end });
    }
    ObjectDefineProperty(this, 'error', {
      __proto__: null,
      value: channelFromMap(nameOrChannels, 'error', 'SyncTracingChannel'),
    });
  }

  get hasSubscribers() {
    return super.hasSubscribers || this.error?.hasSubscribers;
  }

  subscribe(handlers) {
    super.subscribe(handlers);
    if (handlers.error) this.error?.subscribe(handlers.error);
  }

  unsubscribe(handlers) {
    let done = super.unsubscribe(handlers);
    if (handlers.error && !this.error?.unsubscribe(handlers.error)) done = false;
    return done;
  }

  wrap(fn, options = { __proto__: null }) {
    validateFunction(fn, 'fn');
    const {
      context,
      wrapArgs,
      mapResult,
      captureError,
      captureResult,
    } = options;
    if (wrapArgs !== undefined) validateFunction(wrapArgs, 'options.wrapArgs');
    if (mapResult !== undefined) validateFunction(mapResult, 'options.mapResult');
    if (captureError !== undefined) validateFunction(captureError, 'options.captureError');
    if (captureResult !== undefined) validateFunction(captureResult, 'options.captureResult');
    const tracingChannel = this;

    return function wrapped(...args) {
      const traceContext = normalizeContext(context, this, args);
      const callContext = getContextSection(traceContext, 'call');

      const invoke = () => {
        const wrappedArgs = wrapArgs ? FunctionPrototypeCall(wrapArgs, this, args, traceContext) : args;
        const result = ReflectApply(fn, this, wrappedArgs);
        const finalResult = mapResult ? FunctionPrototypeCall(mapResult, this, result, traceContext) : result;
        if (captureResult !== undefined) {
          FunctionPrototypeCall(captureResult, this, finalResult, traceContext);
        }
        callContext.result = finalResult;
        return finalResult;
      };

      if (!tracingChannel.hasSubscribers) {
        return invoke();
      }

      // eslint-disable-next-line no-unused-vars
      using scope = tracingChannel.withScope(traceContext);
      try {
        return invoke();
      } catch (err) {
        if (captureError !== undefined) {
          FunctionPrototypeCall(captureError, this, err, traceContext);
        }
        callContext.error = err;
        tracingChannel.error.publish(traceContext);
        throw err;
      }
    };
  }
}

class CallbackTracingChannel extends BoundedChannel {
  constructor(nameOrChannels) {
    if (typeof nameOrChannels === 'string') {
      super({
        start: channel(`tracing:${nameOrChannels}:asyncStart`),
        end: channel(`tracing:${nameOrChannels}:asyncEnd`),
      });
    } else if (typeof nameOrChannels === 'object' && nameOrChannels !== null) {
      super({
        start: nameOrChannels.asyncStart,
        end: nameOrChannels.asyncEnd,
      });
    } else {
      throw new ERR_INVALID_ARG_TYPE('nameOrChannels',
                                     ['string', 'object', 'CallbackTracingChannel'],
                                     nameOrChannels);
    }
    ObjectDefineProperty(this, 'asyncStart', {
      __proto__: null,
      get() { return this.start; },
    });
    ObjectDefineProperty(this, 'asyncEnd', {
      __proto__: null,
      get() { return this.end; },
    });
    ObjectDefineProperty(this, 'error', {
      __proto__: null,
      value: channelFromMap(nameOrChannels, 'error', 'CallbackTracingChannel'),
    });
  }

  get hasSubscribers() {
    return super.hasSubscribers || this.error?.hasSubscribers;
  }

  subscribe(handlers) {
    super.subscribe({ start: handlers.asyncStart, end: handlers.asyncEnd });
    if (handlers.error) this.error?.subscribe(handlers.error);
  }

  unsubscribe(handlers) {
    let done = super.unsubscribe({ start: handlers.asyncStart, end: handlers.asyncEnd });
    if (handlers.error && !this.error?.unsubscribe(handlers.error)) done = false;
    return done;
  }

  wrap(callback, options = { __proto__: null }) {
    validateFunction(callback, 'callback');
    const {
      context,
      mapOutcome,
      captureError,
      captureResult,
    } = options;
    if (mapOutcome !== undefined) validateFunction(mapOutcome, 'options.mapOutcome');
    if (captureError !== undefined) validateFunction(captureError, 'options.captureError');
    if (captureResult !== undefined) validateFunction(captureResult, 'options.captureResult');
    const tracingChannel = this;

    return function wrapped(...args) {
      const traceContext = normalizeContext(context, this, args);
      const callbackContext = getContextSection(traceContext, 'callback');

      try {
        applyCallbackOutcome(callbackContext, normalizeCallbackOutcome(args, mapOutcome, this));
        if ('error' in callbackContext && captureError !== undefined) {
          FunctionPrototypeCall(captureError, this, callbackContext.error, traceContext);
        }
        if ('result' in callbackContext && captureResult !== undefined) {
          FunctionPrototypeCall(captureResult, this, callbackContext.result, traceContext);
        }
      } catch (err) {
        callbackContext.error = err;
        if (tracingChannel.hasSubscribers) {
          tracingChannel.error.publish(traceContext);
        }
        throw err;
      }

      if (tracingChannel.hasSubscribers && callbackContext.error != null) {
        tracingChannel.error.publish(traceContext);
      }

      if (!tracingChannel.hasSubscribers) {
        return ReflectApply(callback, this, args);
      }

      // Using withScope here enables manual context failure recovery
      // eslint-disable-next-line no-unused-vars
      using scope = tracingChannel.withScope(traceContext);
      return ReflectApply(callback, this, args);
    };
  }

  wrapArgs(args, index = -1, options = { __proto__: null }) {
    const callback = ArrayPrototypeAt(args, index);
    validateFunction(callback, 'callback');
    const wrappedArgs = ArrayPrototypeSlice(args);
    ArrayPrototypeSplice(wrappedArgs, index, 1, this.wrap(callback, options));
    return wrappedArgs;
  }
}

class PromiseTracingChannel {
  constructor(nameOrChannels) {
    defineTracingChannels(this, nameOrChannels, asyncTraceEvents);
  }

  get hasSubscribers() {
    return tracingChannelHasSubscribers(this, asyncTraceEvents);
  }

  subscribe(handlers) {
    tracingChannelSubscribe(this, handlers, asyncTraceEvents);
  }

  unsubscribe(handlers) {
    return tracingChannelUnsubscribe(this, handlers, asyncTraceEvents);
  }

  wrap(value, options = { __proto__: null }) {
    const {
      context,
      mapResult,
      captureError,
      captureResult,
      warningTarget,
    } = options;
    if (mapResult !== undefined) validateFunction(mapResult, 'options.mapResult');
    if (captureError !== undefined) validateFunction(captureError, 'options.captureError');
    if (captureResult !== undefined) validateFunction(captureResult, 'options.captureResult');
    const traceContext = normalizeContext(context, undefined, [value]);
    const promiseContext = getContextSection(traceContext, 'promise');
    const tracingChannel = this;

    // If the return value is not a thenable, then return it with a warning.
    // Do not publish to asyncStart/asyncEnd.
    if (typeof value?.then !== 'function') {
      emitNonThenableWarning(warningTarget ?? {});
      return value;
    }

    function publishAsyncBoundary() {
      tracingChannel.asyncStart.publish(traceContext);
      // TODO: Is there a way to have asyncEnd _after_ the continuation?
      tracingChannel.asyncEnd.publish(traceContext);
    }

    function reject(err) {
      if (captureError !== undefined) {
        captureError(err, traceContext);
      }
      promiseContext.error = err;
      if (tracingChannel.hasSubscribers) {
        tracingChannel.error.publish(traceContext);
        publishAsyncBoundary();
      }
      throw err;
    }

    function resolve(result) {
      try {
        const finalResult = mapResult ? mapResult(result, traceContext) : result;
        if (captureResult !== undefined) {
          captureResult(finalResult, traceContext);
        }
        promiseContext.result = finalResult;
        if (tracingChannel.hasSubscribers) {
          publishAsyncBoundary();
        }
        return finalResult;
      } catch (err) {
        if (captureError !== undefined) {
          captureError(err, traceContext);
        }
        promiseContext.error = err;
        if (tracingChannel.hasSubscribers) {
          tracingChannel.error.publish(traceContext);
          publishAsyncBoundary();
        }
        throw err;
      }
    }

    return value.then(resolve, reject);
  }
}

function wrapIterator(tracingChannel, iterator, options, symbol) {
  const {
    context,
    mapResult,
  } = options;
  if (mapResult !== undefined) validateFunction(mapResult, 'options.mapResult');
  const traceContext = normalizeContext(context, undefined, [iterator]);
  const iteratorContext = getContextSection(traceContext, 'iterator');
  const wrapped = { __proto__: iterator };

  function createMethod(name) {
    const method = iterator?.[name];
    if (typeof method !== 'function') {
      return;
    }

    ObjectDefineProperty(wrapped, name, {
      __proto__: null,
      value(...args) {
        iteratorContext.method = name;
        iteratorContext.args = ArrayPrototypeSlice(args);

        function settleResult(result) {
          try {
            const finalResult = mapResult ? mapResult(result, traceContext) : result;
            iteratorContext.result = finalResult;
            if (tracingChannel.hasSubscribers) {
              tracingChannel[finalResult?.done ? 'return' : 'yield'].publish(traceContext);
            }
            return finalResult;
          } catch (err) {
            iteratorContext.error = err;
            if (tracingChannel.hasSubscribers) {
              tracingChannel.error.publish(traceContext);
            }
            throw err;
          }
        }

        function handleError(err) {
          iteratorContext.error = err;
          if (tracingChannel.hasSubscribers) {
            tracingChannel.error.publish(traceContext);
          }
          throw err;
        }

        try {
          const result = ReflectApply(method, iterator, args);
          if (symbol === SymbolIterator || typeof result?.then !== 'function') {
            return settleResult(result);
          }
          return result.then(settleResult, handleError);
        } catch (err) {
          return handleError(err);
        }
      },
    });
  }

  createMethod('next');
  createMethod('return');
  createMethod('throw');

  ObjectDefineProperty(wrapped, symbol, {
    __proto__: null,
    value() {
      return this;
    },
  });

  return wrapped;
}

class SyncIteratorTracingChannel {
  constructor(nameOrChannels) {
    if (typeof nameOrChannels === 'string') {
      ObjectDefineProperty(this, 'yield', {
        __proto__: null,
        value: channel(`tracing:${nameOrChannels}:syncYield`),
      });
      ObjectDefineProperty(this, 'return', {
        __proto__: null,
        value: channel(`tracing:${nameOrChannels}:return`),
      });
      ObjectDefineProperty(this, 'error', {
        __proto__: null,
        value: channel(`tracing:${nameOrChannels}:error`),
      });
      return;
    }

    if (typeof nameOrChannels === 'object' && nameOrChannels !== null) {
      ObjectDefineProperty(this, 'yield', {
        __proto__: null,
        value: channelFromMap(nameOrChannels, 'yield', 'SyncIteratorTracingChannel'),
      });
      ObjectDefineProperty(this, 'return', {
        __proto__: null,
        value: channelFromMap(nameOrChannels, 'return', 'SyncIteratorTracingChannel'),
      });
      ObjectDefineProperty(this, 'error', {
        __proto__: null,
        value: channelFromMap(nameOrChannels, 'error', 'SyncIteratorTracingChannel'),
      });
      return;
    }

    throw new ERR_INVALID_ARG_TYPE('nameOrChannels',
                                   ['string', 'object', 'SyncIteratorTracingChannel'],
                                   nameOrChannels);
  }

  get hasSubscribers() {
    return tracingChannelHasSubscribers(this, iteratorTraceEvents);
  }

  subscribe(handlers) {
    tracingChannelSubscribe(this, handlers, iteratorTraceEvents);
  }

  unsubscribe(handlers) {
    return tracingChannelUnsubscribe(this, handlers, iteratorTraceEvents);
  }

  wrap(iterator, options = { __proto__: null }) {
    return wrapIterator(this, iterator, options, SymbolIterator);
  }
}

class AsyncIteratorTracingChannel {
  constructor(nameOrChannels) {
    if (typeof nameOrChannels === 'string') {
      ObjectDefineProperty(this, 'yield', {
        __proto__: null,
        value: channel(`tracing:${nameOrChannels}:asyncYield`),
      });
      ObjectDefineProperty(this, 'return', {
        __proto__: null,
        value: channel(`tracing:${nameOrChannels}:return`),
      });
      ObjectDefineProperty(this, 'error', {
        __proto__: null,
        value: channel(`tracing:${nameOrChannels}:error`),
      });
      return;
    }

    if (typeof nameOrChannels === 'object' && nameOrChannels !== null) {
      ObjectDefineProperty(this, 'yield', {
        __proto__: null,
        value: channelFromMap(nameOrChannels, 'yield', 'AsyncIteratorTracingChannel'),
      });
      ObjectDefineProperty(this, 'return', {
        __proto__: null,
        value: channelFromMap(nameOrChannels, 'return', 'AsyncIteratorTracingChannel'),
      });
      ObjectDefineProperty(this, 'error', {
        __proto__: null,
        value: channelFromMap(nameOrChannels, 'error', 'AsyncIteratorTracingChannel'),
      });
      return;
    }

    throw new ERR_INVALID_ARG_TYPE('nameOrChannels',
                                   ['string', 'object', 'AsyncIteratorTracingChannel'],
                                   nameOrChannels);
  }

  get hasSubscribers() {
    return tracingChannelHasSubscribers(this, iteratorTraceEvents);
  }

  subscribe(handlers) {
    tracingChannelSubscribe(this, handlers, iteratorTraceEvents);
  }

  unsubscribe(handlers) {
    return tracingChannelUnsubscribe(this, handlers, iteratorTraceEvents);
  }

  wrap(iterator, options = { __proto__: null }) {
    return wrapIterator(this, iterator, options, SymbolAsyncIterator);
  }
}

class TracingChannel {
  #syncChannel;
  #callbackChannel;

  constructor(nameOrChannels) {
    defineTracingChannels(this, nameOrChannels, traceEvents);

    this.#syncChannel = new SyncTracingChannel(this);
    this.#callbackChannel = new CallbackTracingChannel(this);
  }

  get hasSubscribers() {
    return tracingChannelHasSubscribers(this, traceEvents);
  }

  subscribe(handlers) {
    tracingChannelSubscribe(this, handlers, traceEvents);
  }

  unsubscribe(handlers) {
    return tracingChannelUnsubscribe(this, handlers, traceEvents);
  }

  wrap(fn, options = { __proto__: null }) {
    validateFunction(fn, 'fn');
    return this.#syncChannel.wrap(fn, options);
  }

  traceSync(fn, context = {}, thisArg, ...args) {
    if (!this.hasSubscribers) {
      return ReflectApply(fn, thisArg, args);
    }

    const { error } = this;

    // eslint-disable-next-line no-unused-vars
    using scope = this.#syncChannel.withScope(context);
    try {
      const result = ReflectApply(fn, thisArg, args);
      context.result = result;
      return result;
    } catch (err) {
      context.error = err;
      error.publish(context);
      throw err;
    }
  }

  tracePromise(fn, context = {}, thisArg, ...args) {
    if (!this.hasSubscribers) {
      const result = ReflectApply(fn, thisArg, args);
      if (typeof result?.then !== 'function') {
        emitNonThenableWarning(fn);
      }
      return result;
    }

    const { error } = this;
    // Use #callbackChannel (start=asyncStart, end=asyncEnd) for the continuation
    // window so that asyncStart stores are properly entered on promise settlement,
    // matching the backward-compatible behaviour of the upstream tracePromise.
    const callbackChannel = this.#callbackChannel;

    function reject(err) {
      context.error = err;
      error.publish(context);
      // eslint-disable-next-line no-unused-vars
      using scope = callbackChannel.withScope(context);
      // TODO: Is there a way to have asyncEnd _after_ the continuation?
      return PromiseReject(err);
    }

    function resolve(result) {
      context.result = result;
      // eslint-disable-next-line no-unused-vars
      using scope = callbackChannel.withScope(context);
      // TODO: Is there a way to have asyncEnd _after_ the continuation?
      return result;
    }

    // eslint-disable-next-line no-unused-vars
    using scope = this.#syncChannel.withScope(context);
    try {
      const result = ReflectApply(fn, thisArg, args);
      // If the return value is not a thenable, return it directly with a warning.
      // Do not publish to asyncStart/asyncEnd.
      if (typeof result?.then !== 'function') {
        emitNonThenableWarning(fn);
        context.result = result;
        return result;
      }
      // For native Promises use PromisePrototypeThen to avoid user overrides.
      if (isPromise(result)) {
        return PromisePrototypeThen(result, resolve, reject);
      }
      // For custom thenables, call .then() directly to preserve the thenable type.
      return result.then(resolve, reject);
    } catch (err) {
      context.error = err;
      error.publish(context);
      throw err;
    }
  }

  traceCallback(fn, position = -1, context = {}, thisArg, ...args) {
    if (!this.hasSubscribers) {
      return ReflectApply(fn, thisArg, args);
    }

    const { error } = this;
    const callbackChannel = this.#callbackChannel;

    function wrappedCallback(err, res) {
      if (err) {
        context.error = err;
        error.publish(context);
      } else {
        context.result = res;
      }

      // eslint-disable-next-line no-unused-vars
      using scope = callbackChannel.withScope(context);
      return ReflectApply(callback, this, arguments);
    }

    const callback = ArrayPrototypeAt(args, position);
    validateFunction(callback, 'callback');
    ArrayPrototypeSplice(args, position, 1, wrappedCallback);

    // eslint-disable-next-line no-unused-vars
    using scope = this.#syncChannel.withScope(context);
    try {
      return ReflectApply(fn, thisArg, args);
    } catch (err) {
      context.error = err;
      error.publish(context);
      throw err;
    }
  }
}

function tracingChannel(nameOrChannels) {
  return new TracingChannel(nameOrChannels);
}

function syncTracingChannel(nameOrChannels) {
  return new SyncTracingChannel(nameOrChannels);
}

function callbackTracingChannel(nameOrChannels) {
  return new CallbackTracingChannel(nameOrChannels);
}

function promiseTracingChannel(nameOrChannels) {
  return new PromiseTracingChannel(nameOrChannels);
}

function syncIteratorTracingChannel(nameOrChannels) {
  return new SyncIteratorTracingChannel(nameOrChannels);
}

function asyncIteratorTracingChannel(nameOrChannels) {
  return new AsyncIteratorTracingChannel(nameOrChannels);
}

dc_binding.linkNativeChannel((name) => channel(name));

module.exports = {
  asyncIteratorTracingChannel,
  boundedChannel,
  callbackTracingChannel,
  channel,
  hasSubscribers,
  promiseTracingChannel,
  subscribe,
  syncIteratorTracingChannel,
  syncTracingChannel,
  tracingChannel,
  unsubscribe,
  BoundedChannel,
  Channel,
};
