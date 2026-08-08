'use strict';
const {
  DateNow,
  JSONStringify,
  ObjectAssign,
  ObjectDefineProperty,
  ObjectKeys,
  SafeWeakMap,
  SymbolFor,
} = primordials;

const { isNativeError } = require('internal/util/types');

/**
 * Symbol for custom serialization.
 * Objects can implement this method to control how they are serialized in logs.
 * Similar to util.inspect.custom.
 * @type {symbol}
 */
const serialize = SymbolFor('nodejs.logger.serialize');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_METHOD_NOT_IMPLEMENTED,
  },
} = require('internal/errors');

const {
  validateObject,
  validateString,
  validateOneOf,
  validateFunction,
} = require('internal/validators');

const Utf8Stream = require('internal/streams/fast-utf8-stream');
const diagnosticsChannel = require('diagnostics_channel');
const { emitExperimentalWarning, kEmptyObject } = require('internal/util');
emitExperimentalWarning('Logger');
const stdSerializers = require('internal/logger/serializers');

/*
* RFC5424 numerical ordering + log4j interface
* @link https://www.rfc-editor.org/rfc/rfc5424.html
*/
const LEVELS = {
  __proto__: null,
  trace: 10,
  debug: 20,
  info: 30,
  warn: 40,
  error: 50,
  fatal: 60,
};

const channel = diagnosticsChannel.channel('log');

const LEVEL_NAMES = ObjectKeys(LEVELS);

function isReservedKey(key) {
  return key === 'level' || key === 'time' || key === 'msg';
}

// Noop function for disabled log levels
function noop() {}

/**
 * Base consumer class for handling log records
 * Consumers subscribe to diagnostics_channel events
 */
class LogConsumer {
  /** @type {number} */
  #levelValue;
  #handler;
  #attached;

  constructor(options = kEmptyObject) {
    validateObject(options, 'options');
    const { level = 'info' } = options;
    validateOneOf(level, 'options.level', LEVEL_NAMES);
    this.#levelValue = LEVELS[level];
    this.#handler = undefined;
    this.#attached = false;

    // Setup level-specific enabled properties for typo safety
    // Allows consumer.info.enabled instead of consumer.enabled('info')
    const levelValue = this.#levelValue;
    for (let i = 0; i < LEVEL_NAMES.length; i++) {
      const lvl = LEVEL_NAMES[i];
      this[lvl] = { __proto__: null, enabled: LEVELS[lvl] >= levelValue };
    }
  }

  /**
   * Attach this consumer to log channel
   */
  attach() {
    if (this.#attached) {
      return;
    }
    this.#attached = true;
    this.#handler = (record) => {
      if (record.levelValue < this.#levelValue) {
        return;
      }
      this.handle(record);
    };
    channel.subscribe(this.#handler);
  }

  /**
   * Detach this consumer from log channels
   */
  detach() {
    if (!this.#attached) {
      return;
    }
    this.#attached = false;
    channel.unsubscribe(this.#handler);
    this.#handler = undefined;
  }

  /**
   * Handle a log record (must be implemented by subclass)
   * @param {object} record
   */
  handle(record) {
    throw new ERR_METHOD_NOT_IMPLEMENTED('LogConsumer.handle()');
  }
}

/**
 * JSON consumer - outputs structured JSON logs
 */
class JSONConsumer extends LogConsumer {
  #stream;
  #fields;
  #bindingsCache;

  constructor(options = kEmptyObject) {
    super(options);
    const {
      stream,
      fields = kEmptyObject,
    } = options;

    validateObject(fields, 'options.fields');

    this.#stream = stream ? this.#createStream(stream) :
      new Utf8Stream({ fd: 1 });
    this.#fields = fields;
    this.#bindingsCache = new SafeWeakMap();
  }

  #createStream(stream) {
    // Fast path: already a Utf8Stream
    if (stream instanceof Utf8Stream) {
      return stream;
    }

    // Number: file descriptor
    if (typeof stream === 'number') {
      return new Utf8Stream({ fd: stream });
    }

    // String: file path
    if (typeof stream === 'string') {
      return new Utf8Stream({ dest: stream });
    }

    if (stream !== null && typeof stream === 'object' &&
        typeof stream.write === 'function' &&
        typeof stream.flush === 'function' &&
        typeof stream.flushSync === 'function' &&
        typeof stream.end === 'function') {
      return stream;
    }

    throw new ERR_INVALID_ARG_TYPE(
      'options.stream',
      ['number', 'string', 'Utf8Stream',
       'Object with write, flush, flushSync, and end methods'],
      stream,
    );
  }

  handle(record) {
    // Start building JSON manually
    // record.level is trusted internal value, record.msg is user input and must be escaped
    let json = `{"level":"${record.level}","time":${record.time},"msg":${JSONStringify(record.msg)}`;

    if (record.name !== undefined) {
      json += `,${JSONStringify('name')}:${JSONStringify(record.name)}`;
    }

    const consumerFields = this.#fields;
    const consumerKeys = ObjectKeys(consumerFields);
    for (let i = 0; i < consumerKeys.length; i++) {
      const key = consumerKeys[i];
      if (isReservedKey(key)) continue;
      const value = consumerFields[key];
      if (value === undefined) continue;
      json += `,${JSONStringify(key)}:${JSONStringify(value)}`;
    }

    // Add bindings, cached per bindings object to avoid rebuilding on every log
    json += this.#serializeBindings(record.bindings);

    // Add log fields
    const fields = record.fields;
    const fieldKeys = ObjectKeys(fields);
    for (let i = 0; i < fieldKeys.length; i++) {
      const key = fieldKeys[i];
      if (isReservedKey(key)) continue;
      const value = fields[key];
      if (value === undefined) continue;
      json += `,${JSONStringify(key)}:${JSONStringify(value)}`;
    }

    json += '}\n';
    this.#stream.write(json);
  }

  #serializeBindings(bindings) {
    if (!bindings || typeof bindings !== 'object') {
      return '';
    }

    const cached = this.#bindingsCache.get(bindings);
    if (cached !== undefined) {
      return cached;
    }

    let result = '';
    const keys = ObjectKeys(bindings);
    for (let i = 0; i < keys.length; i++) {
      const key = keys[i];
      if (isReservedKey(key)) continue;
      const value = bindings[key];
      if (value === undefined) continue;
      result += `,${JSONStringify(key)}:${JSONStringify(value)}`;
    }

    this.#bindingsCache.set(bindings, result);
    return result;
  }

  /**
   * Flush pending writes
   * @param {Function} callback
   */
  flush(callback) {
    this.#stream.flush(callback);
  }

  /**
   * Flush pending writes synchronously
   */
  flushSync() {
    this.#stream.flushSync();
  }

  /**
   * Close the consumer
   */
  end() {
    this.detach();
    this.#stream.end();
    this.#stream = null;
  }
}

/**
 * Logger class
 */
class Logger {
  #level;
  #levelValue;
  #bindings;
  #name;
  #serializers;

  /**
   * Create a new Logger instance
   * @param {object} [options]
   * @param {string} [options.level] - Minimum log level (default: 'info')
   * @param {string} [options.name] - Optional logger name/namespace
   * @param {object} [options.bindings] - Context fields (default: {})
   * @param {object} [options.serializers] - Custom serializers (default: {})
   */
  constructor(options = kEmptyObject) {
    validateObject(options, 'options');
    const {
      level = 'info',
      name,
      bindings = kEmptyObject,
      serializers = kEmptyObject,
    } = options;

    validateOneOf(level, 'options.level', LEVEL_NAMES);

    if (name !== undefined) {
      validateString(name, 'options.name');
    }

    validateObject(bindings, 'options.bindings');
    validateObject(serializers, 'options.serializers');

    // Validate serializers are functions (use ObjectKeys to avoid prototype chain)
    const serializerKeys = ObjectKeys(serializers);
    for (const key of serializerKeys) {
      validateFunction(serializers[key], `options.serializers.${key}`);
    }

    this.#level = level;
    this.#levelValue = LEVELS[level];
    this.#name = name;

    // Create serializers object with default err serializer
    this.#serializers = { __proto__: null };
    // Add default error serializers (can be overridden)
    this.#serializers.err = stdSerializers.err;
    this.#serializers.error = stdSerializers.err;

    // Add custom serializers (use serializerKeys from validation above)
    for (const key of serializerKeys) {
      this.#serializers[key] = serializers[key];
    }

    // Apply serializers to bindings so [serialize]() is called on binding values
    this.#bindings = this.#applySerializers(bindings);


    this.#setLogMethods();
  }

  /**
   * Setup log methods with enabled getters.
   *
   * Creates the following methods on the logger instance:
   * - trace(msgOrObj, fields) - Log at trace level
   * - debug(msgOrObj, fields) - Log at debug level
   * - info(msgOrObj, fields) - Log at info level
   * - warn(msgOrObj, fields) - Log at warn level
   * - error(msgOrObj, fields) - Log at error level
   * - fatal(msgOrObj, fields) - Log at fatal level
   *
   * Each method also has an `.enabled` getter for performance checks.
   * @private
   */
  #setLogMethods() {
    const levelValue = this.#levelValue;

    // Setup each log level method with .enabled getter
    this.trace = this.#createLogMethod('trace', LEVELS.trace, levelValue);
    this.debug = this.#createLogMethod('debug', LEVELS.debug, levelValue);
    this.info = this.#createLogMethod('info', LEVELS.info, levelValue);
    this.warn = this.#createLogMethod('warn', LEVELS.warn, levelValue);
    this.error = this.#createLogMethod('error', LEVELS.error, levelValue);
    this.fatal = this.#createLogMethod('fatal', LEVELS.fatal, levelValue);
  }

  /**
   * Create a log method with .enabled getter
   * @param {string} level - Log level name
   * @param {number} methodLevelValue - Numeric value of this level
   * @param {number} loggerLevelValue - Logger's minimum level value
   * @returns {Function} Log method with .enabled property
   * @private
   */
  #createLogMethod(level, methodLevelValue, loggerLevelValue) {
    const enabled = methodLevelValue >= loggerLevelValue;

    let fn;
    if (enabled) {
      // Create bound log function
      fn = (msgOrObj, fields) => {
        this.#log(level, methodLevelValue, msgOrObj, fields);
      };
    } else {
      // Use noop for disabled levels
      fn = noop;
    }

    // Add .enabled getter
    ObjectDefineProperty(fn, 'enabled', {
      __proto__: null,
      value: enabled,
      writable: false,
      enumerable: true,
      configurable: false,
    });

    return fn;
  }

  /**
   * Create a child logger with additional context
   * @param {object} bindings - Context to add to all child logs
   * @param {object} [options] - Optional overrides
   * @param {object} [options.serializers] - Custom serializers for child logger
   * @param {string} [options.level] - Log level for child logger
   * @returns {Logger} Child logger instance
   */
  child(bindings, options = kEmptyObject) {
    validateObject(bindings, 'bindings');
    validateObject(options, 'options');

    // Handle serializers inheritance
    let childSerializers;
    if (options.serializers) {
      validateObject(options.serializers, 'options.serializers');

      // Create new serializers object
      childSerializers = { __proto__: null };

      // Copy parent serializers (safe: this.#serializers has null prototype)
      const parentKeys = ObjectKeys(this.#serializers);
      for (let i = 0; i < parentKeys.length; i++) {
        const key = parentKeys[i];
        childSerializers[key] = this.#serializers[key];
      }

      // Override with child serializers (use ObjectKeys to avoid prototype chain)
      const childKeys = ObjectKeys(options.serializers);
      for (let i = 0; i < childKeys.length; i++) {
        const key = childKeys[i];
        validateFunction(options.serializers[key], `options.serializers.${key}`);
        childSerializers[key] = options.serializers[key];
      }
    }

    const childLogger = new Logger({
      __proto__: null,
      level: options.level ?? this.#level,
      serializers: childSerializers ?? this.#serializers,
    });

    childLogger.#bindings = ObjectAssign(
      { __proto__: null },
      this.#bindings,
      childLogger.#applySerializers(bindings),
    );

    return childLogger;
  }

  /**
   * Serialize a single value using serialize symbol and/or field serializer
   * Uses stacked behavior: first apply [serialize](), then field serializer
   * @param {*} value - Value to serialize
   * @param {string} key - Field key for field-specific serializer lookup
   * @returns {*} Serialized value
   * @private
   */
  #serializeValue(value, key) {
    let result = value;

    // Step 1: Apply [serialize](), if present
    if (result !== null && typeof result === 'object' &&
        typeof result[serialize] === 'function') {
      result = result[serialize]();
    }

    // Step 2: Apply field-specific serializer, if present
    if (this.#serializers[key]) {
      result = this.#serializers[key](result);
    }

    return result;
  }

  /**
   * Apply serializers to an object's properties
   * @param {object} obj - Object to serialize
   * @returns {object} Serialized object with applied serializers
   * @private
   */
  #applySerializers(obj) {
    if (!obj || typeof obj !== 'object') {
      return obj;
    }

    const serialized = { __proto__: null };

    const keys = ObjectKeys(obj);
    for (const key of keys) {
      serialized[key] = this.#serializeValue(obj[key], key);
    }

    return serialized;
  }

  /**
   * Internal log method
   * @param {string} level - Log level
   * @param {number} levelValue - Numeric log level
   * @param {string|object} msgOrObj - Message string or object with msg property
   * @param {object} [fields] - Optional fields to merge
   * @private
   */
  #log(level, levelValue, msgOrObj, fields) {
    if (levelValue < this.#levelValue) {
      return;
    }

    if (!channel.hasSubscribers) {
      return;
    }

    if (typeof msgOrObj !== 'string' && !isNativeError(msgOrObj)) {
      validateObject(msgOrObj, 'obj');
      validateString(msgOrObj.msg, 'obj.msg');
    }
    if (fields !== undefined) {
      validateObject(fields, 'fields');
    }

    let msg;
    let logFields;

    if (isNativeError(msgOrObj)) {
      msg = msgOrObj.message ?? '';
      logFields = {
        __proto__: null,
        err: this.#serializers.err(msgOrObj),
      };

      // Apply serializers to additional fields
      if (fields) {
        const serializedFields = this.#applySerializers(fields);
        const keys = ObjectKeys(serializedFields);
        for (const key of keys) {
          logFields[key] = serializedFields[key];
        }
      }
    } else if (typeof msgOrObj === 'string') {
      msg = msgOrObj;
      // Apply serializers to fields
      logFields = fields ? this.#applySerializers(fields) : kEmptyObject;
    } else {
      const { msg: extractedMsg, ...restFields } = msgOrObj;
      msg = extractedMsg;

      // Apply serializers to object fields
      logFields = this.#applySerializers(restFields);
    }

    const record = {
      __proto__: null,
      level,
      levelValue,
      name: this.#name,
      msg,
      time: DateNow(),
      bindings: this.#bindings,
      fields: logFields,
    };

    channel.publish(record);
  }

}

module.exports = {
  __proto__: null,
  Logger,
  LogConsumer,
  JSONConsumer,
  stdSerializers,
  serialize,
};
