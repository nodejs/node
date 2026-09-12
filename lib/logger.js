'use strict';

const {
  ArrayPrototypeIncludes,
  ArrayPrototypePop,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  DateNow,
  FunctionPrototypeCall,
  JSONStringify,
  ObjectAssign,
  ObjectDefineProperties,
  ObjectFreeze,
  ObjectKeys,
  ReflectOwnKeys,
  SafeWeakMap,
  String,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_RETURN_VALUE,
    ERR_INVALID_STATE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');
const {
  validateArray,
  validateBoolean,
  validateFunction,
  validateInteger,
  validateObject,
  validateOneOf,
  validateString,
} = require('internal/validators');
const {
  emitExperimentalWarning,
  isError,
  kEmptyObject,
} = require('internal/util');
const Utf8Stream = require('internal/streams/fast-utf8-stream');
const { channel: diagnosticsChannel } = require('diagnostics_channel');
const EventEmitter = require('events');

const kDestinations = ['stdout', 'stderr'];
const kEmptyAttributes = ObjectFreeze({ __proto__: null });
const kEmptyBindings = ObjectFreeze({ __proto__: null });

function createLevel(name, value) {
  return ObjectFreeze({ __proto__: null, name, value });
}

const levels = ObjectFreeze({
  __proto__: null,
  trace: createLevel('trace', 10),
  debug: createLevel('debug', 20),
  info: createLevel('info', 30),
  warn: createLevel('warn', 40),
  error: createLevel('error', 50),
  fatal: createLevel('fatal', 60),
});
const kLevelNames = ObjectKeys(levels);

function normalizeLevel(level, name = 'level') {
  if (typeof level === 'string') {
    validateOneOf(level, name, kLevelNames);
    return levels[level];
  }

  validateObject(level, name);
  const { name: levelName, value } = level;
  validateString(levelName, `${name}.name`);
  validateInteger(value, `${name}.value`);
  return createLevel(levelName, value);
}

function copyObject(value, resolveFunctions = false) {
  const copy = ObjectAssign({ __proto__: null }, value);
  if (resolveFunctions) {
    const keys = ReflectOwnKeys(copy);
    for (let i = 0; i < keys.length; i++) {
      const key = keys[i];
      const entry = copy[key];
      if (typeof entry === 'function') {
        copy[key] = FunctionPrototypeCall(entry);
      }
    }
  }
  return ObjectFreeze(copy);
}

function hasFunction(value) {
  const keys = ReflectOwnKeys(value);
  for (let i = 0; i < keys.length; i++) {
    if (typeof value[keys[i]] === 'function') {
      return true;
    }
  }
  return false;
}

function isProvider(value) {
  return value !== null &&
    (typeof value === 'object' || typeof value === 'function') &&
    typeof value.log === 'function';
}

function validateProvider(provider, name = 'provider') {
  if (provider === null ||
      (typeof provider !== 'object' && typeof provider !== 'function')) {
    throw new ERR_INVALID_ARG_TYPE(name, 'Provider', provider);
  }

  validateFunction(provider.log, `${name}.log`);
  if (provider.isEnabled !== undefined) {
    validateFunction(provider.isEnabled, `${name}.isEnabled`);
  }
}

function isProviderEnabled(provider, level, context) {
  const isEnabled = provider.isEnabled;
  return isEnabled === undefined || FunctionPrototypeCall(
    isEnabled,
    provider,
    level,
    context,
  ) !== false;
}

function serializeError(error) {
  const serialized = ObjectAssign({ __proto__: null }, error, {
    __proto__: null,
    message: error.message,
    name: error.name,
    stack: error.stack,
  });
  if (error.code !== undefined) serialized.code = error.code;
  if (error.cause !== undefined) serialized.cause = error.cause;
  if (error.errors !== undefined) serialized.errors = error.errors;
  return serialized;
}

function defaultSerializer(event) {
  const ancestors = [];
  const replacements = new SafeWeakMap();

  return JSONStringify(event, function replacer(_key, value) {
    if (typeof value === 'bigint') return String(value);
    if (value === null || typeof value !== 'object') return value;

    const holder = replacements.get(this) ?? this;
    while (ancestors.length > 0 &&
           ancestors[ancestors.length - 1] !== holder) {
      ArrayPrototypePop(ancestors);
    }
    if (ArrayPrototypeIncludes(ancestors, value)) return '[Circular]';
    ArrayPrototypePush(ancestors, value);

    if (isError(value)) {
      const serialized = serializeError(value);
      replacements.set(serialized, value);
      return serialized;
    }
    return value;
  });
}

/**
 * A provider that fans log events out to multiple providers.
 */
class AggregateProvider {
  #providers;

  constructor(providers) {
    validateArray(providers, 'providers');
    const copy = ArrayPrototypeSlice(providers);
    for (let i = 0; i < copy.length; i++) {
      validateProvider(copy[i], `providers[${i}]`);
    }
    this.#providers = ObjectFreeze(copy);
  }

  get providers() {
    return this.#providers;
  }

  isEnabled(level, context) {
    for (let i = 0; i < this.#providers.length; i++) {
      if (isProviderEnabled(this.#providers[i], level, context)) return true;
    }
    return false;
  }

  log(event, context) {
    for (let i = 0; i < this.#providers.length; i++) {
      const provider = this.#providers[i];
      if (isProviderEnabled(provider, event.level, context)) {
        FunctionPrototypeCall(provider.log, provider, event, context);
      }
    }
  }
}

/**
 * A provider that writes newline-delimited JSON to stdout or stderr.
 */
class ConsoleProvider {
  #destination;
  #flatten;
  #level;
  #pid;
  #serializer;
  #stream;

  constructor(options = kEmptyObject) {
    validateObject(options, 'options');
    const {
      destination = 'stdout',
      flatten = false,
      level = 'info',
      maxLength,
      minLength,
      periodicFlush,
      pid = false,
      serializer = defaultSerializer,
      sync,
    } = options;

    validateOneOf(destination, 'options.destination', kDestinations);
    validateBoolean(flatten, 'options.flatten');
    validateBoolean(pid, 'options.pid');
    validateFunction(serializer, 'options.serializer');
    this.#destination = destination;
    this.#flatten = flatten;
    this.#level = normalizeLevel(level, 'options.level');
    this.#pid = pid;
    this.#serializer = serializer;
    this.#stream = new Utf8Stream({
      __proto__: null,
      fd: destination === 'stdout' ? 1 : 2,
      maxLength,
      minLength,
      periodicFlush,
      sync,
    });
  }

  get destination() {
    return this.#destination;
  }

  get flatten() {
    return this.#flatten;
  }

  get level() {
    return this.#level;
  }

  set level(level) {
    this.#level = normalizeLevel(level);
  }

  get pid() {
    return this.#pid;
  }

  get serializer() {
    return this.#serializer;
  }

  get stream() {
    return this.#stream;
  }

  isEnabled(level) {
    return level.value >= this.#level.value;
  }

  log(event) {
    if (this.isEnabled(event.level)) {
      let record = event;
      if (this.#flatten || this.#pid) {
        record = this.#flatten ? ObjectAssign(
          { __proto__: null },
          event.bindings,
          event.attributes,
        ) : ObjectAssign({ __proto__: null }, event);
        if (this.#flatten) {
          record.level = event.level.value;
          record.levelName = event.level.name;
          record.message = event.message;
          record.name = event.name;
          record.timestamp = event.timestamp;
        }
        if (this.#pid) record.pid = process.pid;
        ObjectFreeze(record);
      }
      const serialized = FunctionPrototypeCall(
        this.#serializer,
        undefined,
        record,
      );
      if (typeof serialized !== 'string') {
        throw new ERR_INVALID_RETURN_VALUE(
          'a string',
          'serializer',
          serialized,
        );
      }
      this.#stream.write(`${serialized}\n`);
    }
  }

  flush(callback) {
    this.#stream.flush(callback);
  }

  flushSync() {
    this.#stream.flushSync();
  }
}

/**
 * A provider that publishes log events to diagnostics channels.
 */
class DiagnosticsProvider {
  #router;
  #routes;

  constructor(channels) {
    if (typeof channels === 'function') {
      this.#router = channels;
      return;
    }

    validateObject(channels, 'channels');
    const names = ReflectOwnKeys(channels);
    const routes = [];
    for (let i = 0; i < names.length; i++) {
      const name = names[i];
      const selector = channels[name];
      validateFunction(selector, `channels[${i}]`);
      ArrayPrototypePush(routes, ObjectFreeze({
        __proto__: null,
        channel: diagnosticsChannel(name),
        selector,
      }));
    }
    this.#routes = ObjectFreeze(routes);
  }

  isEnabled(level) {
    if (this.#router !== undefined) {
      const channel = this.#getChannel(level);
      return channel?.hasSubscribers === true;
    }

    for (let i = 0; i < this.#routes.length; i++) {
      const route = this.#routes[i];
      if (route.channel.hasSubscribers &&
          FunctionPrototypeCall(route.selector, undefined, level)) {
        return true;
      }
    }
    return false;
  }

  log(event) {
    if (this.#router !== undefined) {
      const channel = this.#getChannel(event.level);
      if (channel?.hasSubscribers) channel.publish(event);
      return;
    }

    for (let i = 0; i < this.#routes.length; i++) {
      const route = this.#routes[i];
      if (route.channel.hasSubscribers &&
          FunctionPrototypeCall(route.selector, undefined, event.level)) {
        route.channel.publish(event);
      }
    }
  }

  #getChannel(level) {
    const name = FunctionPrototypeCall(this.#router, undefined, level);
    return name === undefined ? undefined : diagnosticsChannel(name);
  }
}

/**
 * An EventEmitter that receives structured log events.
 */
class EventProvider extends EventEmitter {
  #level;

  constructor(options = kEmptyObject) {
    validateObject(options, 'options');
    const { level = 'trace' } = options;
    const normalizedLevel = normalizeLevel(level, 'options.level');

    super();
    this.#level = normalizedLevel;
  }

  get level() {
    return this.#level;
  }

  set level(level) {
    this.#level = normalizeLevel(level);
  }

  isEnabled(level) {
    return level.value >= this.#level.value;
  }

  log(event) {
    if (this.isEnabled(event.level)) {
      const eventName = event.level.name;
      this.emit(this.listenerCount(eventName) > 0 ? eventName : 'log', event);
    }
  }
}

let isLogger;

class Logger {
  #bindings;
  #context;
  #hasFunction;
  #name;
  #provider;

  static {
    isLogger = (obj) => {
      return obj !== null && typeof obj === 'object' && #provider in obj;
    };
  }

  constructor(providerOrOptions, options) {
    emitExperimentalWarning('Logger');

    let provider = providerOrOptions;
    if (provider == null) {
      provider = getDefaultProvider();
    } else if (options === undefined &&
               typeof provider === 'object' &&
               !isProvider(provider)) {
      options = provider;
      provider = getDefaultProvider();
    }

    options ??= kEmptyObject;
    validateProvider(provider);
    validateObject(options, 'options');

    const {
      bindings = kEmptyBindings,
      name,
    } = options;
    validateObject(bindings, 'options.bindings');
    if (name !== undefined) {
      validateString(name, 'options.name');
    }

    this.#provider = provider;
    this.#name = name;
    this.#bindings = copyObject(bindings);
    this.#hasFunction = hasFunction(this.#bindings);
    this.#context = ObjectFreeze({
      __proto__: null,
      bindings: this.#bindings,
      name: this.#name,
    });
  }

  get provider() {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    return this.#provider;
  }

  get name() {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    return this.#name;
  }

  get bindings() {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    return this.#bindings;
  }

  isEnabled(level) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    return this.#isEnabled(normalizeLevel(level));
  }

  log(level, message, attributes = kEmptyAttributes) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    this.#log(normalizeLevel(level), message, attributes);
  }

  trace(message, attributes = kEmptyAttributes) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    this.#log(levels.trace, message, attributes);
  }

  debug(message, attributes = kEmptyAttributes) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    this.#log(levels.debug, message, attributes);
  }

  info(message, attributes = kEmptyAttributes) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    this.#log(levels.info, message, attributes);
  }

  warn(message, attributes = kEmptyAttributes) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    this.#log(levels.warn, message, attributes);
  }

  error(message, attributes = kEmptyAttributes) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    this.#log(levels.error, message, attributes);
  }

  fatal(message, attributes = kEmptyAttributes) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    this.#log(levels.fatal, message, attributes);
  }

  child(bindings, options = kEmptyObject) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    validateObject(bindings, 'bindings');
    validateObject(options, 'options');

    const { name = this.#name } = options;
    if (name !== undefined) {
      validateString(name, 'options.name');
    }

    return new Logger(this.#provider, {
      __proto__: null,
      bindings: ObjectAssign({ __proto__: null }, this.#bindings, bindings),
      name,
    });
  }

  #isEnabled(level) {
    return isProviderEnabled(this.#provider, level, this.#context);
  }

  #log(level, message, attributes) {
    if (!this.#isEnabled(level)) {
      return;
    }

    validateObject(attributes, 'attributes');

    const event = ObjectFreeze({
      __proto__: null,
      attributes: attributes === kEmptyAttributes ?
        attributes : copyObject(attributes, true),
      bindings: this.#hasFunction ?
        copyObject(this.#bindings, true) : this.#bindings,
      level,
      message,
      name: this.#name,
      timestamp: DateNow(),
    });

    FunctionPrototypeCall(
      this.#provider.log,
      this.#provider,
      event,
      this.#context,
    );
  }
}

function create(provider, options) {
  return new Logger(provider, options);
}

let defaultProvider;

function getDefaultProvider() {
  defaultProvider ??= new ConsoleProvider();
  return defaultProvider;
}

let settingDefaultProvider = false;

function setDefaultProvider(provider) {
  if (settingDefaultProvider) {
    throw new ERR_INVALID_STATE('Already setting the default provider');
  }
  settingDefaultProvider = true;
  try {
    // Note that we validate the provider after the settingDefaultProvider
    // check. That is to prevent re-entrant calls that might happen when
    // we try to access the properties of the provider that is given.
    validateProvider(provider, 'defaultProvider');
    if (provider !== defaultProvider &&
      process.listenerCount('defaultLoggerProviderChanged')) {
      process.emit('defaultLoggerProviderChanged', provider, defaultProvider);
    }
    defaultProvider = provider;
  } finally {
    settingDefaultProvider = false;
  }
}

function getDescriptor(value) {
  return {
    __proto__: null,
    value,
    enumerable: true,
    writable: true,
    configurable: true,
  };
}

ObjectDefineProperties(create, {
  __proto__: null,
  AggregateProvider: getDescriptor(AggregateProvider),
  ConsoleProvider: getDescriptor(ConsoleProvider),
  DiagnosticsProvider: getDescriptor(DiagnosticsProvider),
  EventProvider: getDescriptor(EventProvider),
  Logger: getDescriptor(Logger),
  create: getDescriptor(create),
  getDefaultProvider: getDescriptor(getDefaultProvider),
  levels: getDescriptor(levels),
  setDefaultProvider: getDescriptor(setDefaultProvider),
});

module.exports = create;
