'use strict';

const {
  ArrayPrototypeAt,
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypePushApply,
  ArrayPrototypeSlice,
  ArrayPrototypeSplice,
  ObjectDefineProperty,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  PromisePrototype,
  PromisePrototypeThen,
  ReflectApply,
  SafeFinalizationRegistry,
  SafeMap,
  SafeSet,
  SymbolDispose,
  SymbolHasInstance,
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

const { WeakReference, kEmptyObject } = require('internal/util');
const { isPromise } = require('internal/util/types');

let bypassStorage;

function getBypassStorage() {
  if (bypassStorage === undefined) {
    const { AsyncLocalStorage } = require('async_hooks');
    bypassStorage = new AsyncLocalStorage();
  }
  return bypassStorage;
}

function withBypassContext(set, fn, thisArg, args) {
  return getBypassStorage().run(
    set,
    () => ReflectApply(fn, thisArg, args),
  );
}

function validateBypassKey(value, name) {
  if (value == null) {
    throw new ERR_INVALID_ARG_TYPE(name, ['object', 'symbol'], value);
  }
  const type = typeof value;
  if (type !== 'object' && type !== 'symbol') {
    throw new ERR_INVALID_ARG_TYPE(name, ['object', 'symbol'], value);
  }
}

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
  channel._bypassSubscribers = null;
  channel._stores = new SafeMap();
  channel._bypassStores = null;
}

function maybeMarkInactive(channel) {
  // When there are no more active subscribers or bound, restore to fast prototype.
  if (!channel._subscribers.length &&
      !channel._stores.size &&
      !channel._bypassSubscribers?.length &&
      !channel._bypassStores?.size) {
    // eslint-disable-next-line no-use-before-define
    ObjectSetPrototypeOf(channel, Channel.prototype);
    channel._subscribers = undefined;
    channel._stores = undefined;
    channel._bypassSubscribers = undefined;
    channel._bypassStores = undefined;
  }
}

class RunStoresScope {
  #stack;

  constructor(activeChannel, data) {
    // eslint-disable-next-line no-restricted-globals
    using stack = new DisposableStack();

    // Normal stores, unchanged from before.
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

    // Bypass stores, only checked when bypass stores exist.
    if (activeChannel._bypassStores) {
      const activeKeys = getBypassStorage().getStore();
      for (const entry of activeChannel._bypassStores.entries()) {
        const store = entry[0];
        const { transform, bypassId } = entry[1];
        if (activeKeys?.has(bypassId)) continue;
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
  subscribe(subscription, options = kEmptyObject) {
    validateFunction(subscription, 'subscription');
    const bypassId = options?.bypassId;

    if (bypassId !== undefined) {
      validateBypassKey(bypassId, 'bypassId');
      // Bypass path, lazy separate array only allocated when needed.
      if (this._bypassSubscribers === null) {
        this._bypassSubscribers = [];
      }
      this._bypassSubscribers = ArrayPrototypeSlice(this._bypassSubscribers);
      ArrayPrototypePush(this._bypassSubscribers, { handler: subscription, bypassId });
    } else {
      // Normal path, plain function call with no extra overhead.
      this._subscribers = ArrayPrototypeSlice(this._subscribers);
      ArrayPrototypePush(this._subscribers, subscription);
    }

    channels.incRef(this.name);
    if (this._index !== undefined) dc_binding.subscribers[this._index]++;
  }

  unsubscribe(subscription) {
    const index = ArrayPrototypeIndexOf(this._subscribers, subscription);
    if (index !== -1) {
      const before = ArrayPrototypeSlice(this._subscribers, 0, index);
      const after = ArrayPrototypeSlice(this._subscribers, index + 1);
      this._subscribers = before;
      ArrayPrototypePushApply(this._subscribers, after);
      channels.decRef(this.name);
      if (this._index !== undefined) dc_binding.subscribers[this._index]--;
      maybeMarkInactive(this);
      return true;
    }

    // Check bypass subscribers
    if (this._bypassSubscribers !== null) {
      let bypassIndex = -1;
      for (let i = 0; i < this._bypassSubscribers.length; i++) {
        if (this._bypassSubscribers[i].handler === subscription) {
          bypassIndex = i;
          break;
        }
      }
      if (bypassIndex !== -1) {
        const before = ArrayPrototypeSlice(this._bypassSubscribers, 0, bypassIndex);
        const after = ArrayPrototypeSlice(this._bypassSubscribers, bypassIndex + 1);
        this._bypassSubscribers = before;
        ArrayPrototypePushApply(this._bypassSubscribers, after);
        if (this._bypassSubscribers.length === 0) {
          this._bypassSubscribers = null;
        }
        channels.decRef(this.name);
        if (this._index !== undefined) dc_binding.subscribers[this._index]--;
        maybeMarkInactive(this);
        return true;
      }
    }

    return false;
  }

  bindStore(store, transform, options = kEmptyObject) {
    const bypassId = options?.bypassId;

    if (bypassId !== undefined) {
      validateBypassKey(bypassId, 'bypassId');
      // Bypass path, lazy separate SafeMap only allocated when needed.
      if (this._bypassStores === null) {
        this._bypassStores = new SafeMap();
      }
      const replacing = this._bypassStores.has(store);
      if (!replacing) {
        channels.incRef(this.name);
        if (this._index !== undefined) dc_binding.subscribers[this._index]++;
      }
      this._bypassStores.set(store, { transform, bypassId });
    } else {
      // No bypassId provided: store as a plain transform function,
      // matching the pre-bypass-feature behavior exactly.
      const replacing = this._stores.has(store);
      if (!replacing) {
        channels.incRef(this.name);
        if (this._index !== undefined) dc_binding.subscribers[this._index]++;
      }
      this._stores.set(store, transform);
    }
  }

  unbindStore(store) {
    if (this._stores.has(store)) {
      this._stores.delete(store);
      channels.decRef(this.name);
      if (this._index !== undefined) dc_binding.subscribers[this._index]--;
      maybeMarkInactive(this);
      return true;
    }

    if (this._bypassStores?.has(store)) {
      this._bypassStores.delete(store);
      if (this._bypassStores.size === 0) {
        this._bypassStores = null;
      }
      channels.decRef(this.name);
      if (this._index !== undefined) dc_binding.subscribers[this._index]--;
      maybeMarkInactive(this);
      return true;
    }

    return false;
  }

  get hasSubscribers() {
    return true;
  }

  publish(data) {
    // Normal path, no ALS lookup, plain function call, zero overhead.
    const subscribers = this._subscribers;
    for (let i = 0; i < subscribers.length; i++) {
      try {
        subscribers[i](data, this.name);
      } catch (err) {
        process.nextTick(() => {
          triggerUncaughtException(err, false);
        });
      }
    }

    // Bypass path, only entered if bypass subscribers exist.
    if (this._bypassSubscribers !== null) {
      const activeKeys = getBypassStorage().getStore();
      const bypassSubscribers = this._bypassSubscribers;
      for (let i = 0; i < bypassSubscribers.length; i++) {
        try {
          const { handler, bypassId } = bypassSubscribers[i];
          if (activeKeys?.has(bypassId)) continue;
          handler(data, this.name);
        } catch (err) {
          process.nextTick(() => {
            triggerUncaughtException(err, false);
          });
        }
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

  subscribe(subscription, options) {
    markActive(this);
    this.subscribe(subscription, options);
  }

  unsubscribe() {
    return false;
  }

  bindStore(store, transform, options) {
    markActive(this);
    this.bindStore(store, transform, options);
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

function assertChannel(value, name) {
  if (!(value instanceof Channel)) {
    throw new ERR_INVALID_ARG_TYPE(name, ['Channel'], value);
  }
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

  subscribe(handlers, options) {
    for (let i = 0; i < boundedEvents.length; ++i) {
      const name = boundedEvents[i];
      if (!handlers[name]) continue;

      this[name]?.subscribe(handlers[name], options);
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

  withScope(context = kEmptyObject) {
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

class TracingChannel {
  #callWindow;
  #continuationWindow;

  constructor(nameOrChannels) {
    // Create a BoundedChannel for start/end (call window)
    if (typeof nameOrChannels === 'string') {
      this.#callWindow = new BoundedChannel(nameOrChannels);
      this.#continuationWindow = new BoundedChannel({
        start: channel(`tracing:${nameOrChannels}:asyncStart`),
        end: channel(`tracing:${nameOrChannels}:asyncEnd`),
      });
    } else if (typeof nameOrChannels === 'object') {
      this.#callWindow = new BoundedChannel({
        start: nameOrChannels.start,
        end: nameOrChannels.end,
      });
      this.#continuationWindow = new BoundedChannel({
        start: nameOrChannels.asyncStart,
        end: nameOrChannels.asyncEnd,
      });
    }

    // Create individual channel for error
    ObjectDefineProperty(this, 'error', {
      __proto__: null,
      value: channelFromMap(nameOrChannels, 'error', 'TracingChannel'),
    });
  }

  get start() {
    return this.#callWindow.start;
  }

  get end() {
    return this.#callWindow.end;
  }

  get asyncStart() {
    return this.#continuationWindow.start;
  }

  get asyncEnd() {
    return this.#continuationWindow.end;
  }

  get hasSubscribers() {
    return this.#callWindow.hasSubscribers ||
      this.#continuationWindow.hasSubscribers ||
      this.error?.hasSubscribers;
  }

  subscribe(handlers, options) {
    // Subscribe to call window (start/end)
    if (handlers.start || handlers.end) {
      this.#callWindow.subscribe({
        start: handlers.start,
        end: handlers.end,
      }, options);
    }

    // Subscribe to continuation window (asyncStart/asyncEnd)
    if (handlers.asyncStart || handlers.asyncEnd) {
      this.#continuationWindow.subscribe({
        start: handlers.asyncStart,
        end: handlers.asyncEnd,
      }, options);
    }

    // Subscribe to error channel
    if (handlers.error) {
      this.error.subscribe(handlers.error, options);
    }
  }

  unsubscribe(handlers) {
    let done = true;

    // Unsubscribe from call window
    if (handlers.start || handlers.end) {
      if (!this.#callWindow.unsubscribe({
        start: handlers.start,
        end: handlers.end,
      })) {
        done = false;
      }
    }

    // Unsubscribe from continuation window
    if (handlers.asyncStart || handlers.asyncEnd) {
      if (!this.#continuationWindow.unsubscribe({
        start: handlers.asyncStart,
        end: handlers.asyncEnd,
      })) {
        done = false;
      }
    }

    // Unsubscribe from error channel
    if (handlers.error) {
      if (!this.error.unsubscribe(handlers.error)) {
        done = false;
      }
    }

    return done;
  }

  traceSync(fn, context = { __proto__: null }, thisArg, ...args) {
    if (!this.hasSubscribers) {
      return ReflectApply(fn, thisArg, args);
    }

    const { error } = this;

    // eslint-disable-next-line no-unused-vars
    using scope = this.#callWindow.withScope(context);
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

  tracePromise(fn, context = { __proto__: null }, thisArg, ...args) {
    if (!this.hasSubscribers) {
      const result = ReflectApply(fn, thisArg, args);
      if (typeof result?.then !== 'function') {
        emitNonThenableWarning(fn);
      }
      return result;
    }

    const { error } = this;
    const continuationWindow = this.#continuationWindow;

    function onReject(err) {
      context.error = err;
      error.publish(context);
      // Use continuation window for asyncStart/asyncEnd
      // eslint-disable-next-line no-unused-vars
      using scope = continuationWindow.withScope(context);
      // TODO: Is there a way to have asyncEnd _after_ the continuation?
    }

    function onRejectWithRethrow(err) {
      onReject(err);
      throw err;
    }

    function onResolve(result) {
      context.result = result;
      // Use continuation window for asyncStart/asyncEnd
      // eslint-disable-next-line no-unused-vars
      using scope = continuationWindow.withScope(context);
      // TODO: Is there a way to have asyncEnd _after_ the continuation?
      return result;
    }

    // eslint-disable-next-line no-unused-vars
    using scope = this.#callWindow.withScope(context);
    try {
      const result = ReflectApply(fn, thisArg, args);
      // If the return value is not a thenable, return it directly with a warning.
      // Do not publish to asyncStart/asyncEnd.
      if (typeof result?.then !== 'function') {
        emitNonThenableWarning(fn);
        context.result = result;
        return result;
      }
      // isPromise() matches sub-classes, but we need to match only direct
      // instances of the native Promise type to safely use PromisePrototypeThen.
      if (isPromise(result) && ObjectGetPrototypeOf(result) === PromisePrototype) {
        return PromisePrototypeThen(result, onResolve, onRejectWithRethrow);
      }
      // For non-native thenables, subscribe to the result but return the
      // original thenable so the consumer can continue handling it directly.
      // Non-native thenables don't have unhandledRejection tracking, so
      // swallowing the rejection here doesn't change existing behaviour.
      result.then(onResolve, onReject);
      return result;
    } catch (err) {
      context.error = err;
      error.publish(context);
      throw err;
    }
  }

  traceCallback(fn, position = -1, context = kEmptyObject, thisArg, ...args) {
    if (!this.hasSubscribers) {
      return ReflectApply(fn, thisArg, args);
    }

    const { error } = this;
    const continuationWindow = this.#continuationWindow;

    function wrappedCallback(err, res) {
      if (err) {
        context.error = err;
        error.publish(context);
      } else {
        context.result = res;
      }

      // Use continuation window for asyncStart/asyncEnd around callback
      // eslint-disable-next-line no-unused-vars
      using scope = continuationWindow.withScope(context);
      return ReflectApply(callback, this, arguments);
    }

    const callback = ArrayPrototypeAt(args, position);
    validateFunction(callback, 'callback');
    ArrayPrototypeSplice(args, position, 1, wrappedCallback);

    // eslint-disable-next-line no-unused-vars
    using scope = this.#callWindow.withScope(context);
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

dc_binding.linkNativeChannel((name) => channel(name));

function bypass(key, fn, thisArg, ...args) {
  validateFunction(fn, 'fn');

  validateBypassKey(key, 'key');

  const currentSet = getBypassStorage().getStore();
  const next = currentSet ? new SafeSet(currentSet) : new SafeSet();
  next.add(key);
  return withBypassContext(next, fn, thisArg, args);
}

module.exports = {
  channel,
  hasSubscribers,
  subscribe,
  bypass,
  tracingChannel,
  unsubscribe,
  boundedChannel,
  Channel,
  BoundedChannel,
};
