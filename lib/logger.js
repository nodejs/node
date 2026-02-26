'use strict';
const {
  DateNow,
  JSONStringify,
  ObjectAssign,
  ObjectDefineProperty,
  ObjectKeys,
  SafeSet,
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

// Create channels for each log level
const channels = {
  __proto__: null,
  trace: diagnosticsChannel.channel('log:trace'),
  debug: diagnosticsChannel.channel('log:debug'),
  info: diagnosticsChannel.channel('log:info'),
  warn: diagnosticsChannel.channel('log:warn'),
  error: diagnosticsChannel.channel('log:error'),
  fatal: diagnosticsChannel.channel('log:fatal'),
};

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

const LEVEL_NAMES = ObjectKeys(LEVELS);

// Noop function for disabled log levels
function noop() {}

/**
 * Base consumer class for handling log records
 * Consumers subscribe to diagnostics_channel events
 */
class LogConsumer {
  /** @type {number} */
  #levelValue;

  constructor(options = kEmptyObject) {
    validateObject(options, 'options');
    const { level = 'info' } = options;
    validateOneOf(level, 'options.level', LEVEL_NAMES);
    this.#levelValue = LEVELS[level];

    // Setup level-specific enabled properties for typo safety
    // Allows consumer.info.enabled instead of consumer.enabled('info')
    const levelValue = this.#levelValue;
    for (let i = 0; i < LEVEL_NAMES.length; i++) {
      const lvl = LEVEL_NAMES[i];
      this[lvl] = { __proto__: null, enabled: LEVELS[lvl] >= levelValue };
    }
  }

  /**
   * Attach this consumer to log channels
   */
  attach() {
    for (const level of LEVEL_NAMES) {
      if (this[level].enabled) {
        channels[level].subscribe(this.handle.bind(this));
      }
    }
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

    // Object: custom stream with write method
    if (typeof stream === 'object' && stream !== null &&
        typeof stream.write === 'function') {
      return stream;
    }

    throw new ERR_INVALID_ARG_TYPE(
      'options.stream',
      ['number', 'string', 'Utf8Stream', 'object with write method'],
      stream,
    );
  }

  handle(record) {
    // Start building JSON manually
    let json = `{"level":"${record.level}","time":${record.time},"msg":"${record.msg}"`;

    // Add consumer fields
    const consumerFields = this.#fields;
    for (const key of ObjectKeys(consumerFields)) {
      const value = consumerFields[key];
      json += typeof value === 'string' ?
        `,"${key}":"${value}"` :
        `,"${key}":${JSONStringify(value)}`;
    }

    // Add pre-serialized bindings
    json += record.bindingsStr;

    // Add log fields
    const fields = record.fields;
    for (const key in fields) {
      const value = fields[key];
      json += typeof value === 'string' ?
        `,"${key}":"${value}"` :
        `,"${key}":${JSONStringify(value)}`;
    }

    json += '}\n';
    this.#stream.write(json);
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
    this.#stream.end();
  }
}

/**
 * Logger class
 */
class Logger {
  #level;
  #levelValue;
  #bindings;
  #bindingsStr; // Pre-serialized bindings JSON string
  #serializers;

  /**
   * Create a new Logger instance
   * @param {object} [options]
   * @param {string} [options.level] - Minimum log level (default: 'info')
   * @param {object} [options.bindings] - Context fields (default: {})
   * @param {object} [options.serializers] - Custom serializers (default: {})
   */
  constructor(options = kEmptyObject) {
    validateObject(options, 'options');
    const {
      level = 'info',
      bindings = kEmptyObject,
      serializers = kEmptyObject,
    } = options;

    validateOneOf(level, 'options.level', LEVEL_NAMES);

    validateObject(bindings, 'options.bindings');
    validateObject(serializers, 'options.serializers');

    // Validate serializers are functions (use ObjectKeys to avoid prototype chain)
    const serializerKeys = ObjectKeys(serializers);
    for (const key of serializerKeys) {
      validateFunction(serializers[key], `options.serializers.${key}`);
    }

    this.#level = level;
    this.#levelValue = LEVELS[level];
    this.#bindings = bindings;

    // Create serializers object with default err serializer
    this.#serializers = { __proto__: null };
    // Add default err serializer (can be overridden)
    this.#serializers.err = stdSerializers.err;

    // Add custom serializers (use serializerKeys from validation above)
    for (const key of serializerKeys) {
      this.#serializers[key] = serializers[key];
    }

    // Pre-serialize bindings
    this.#bindingsStr = this.#serializeBindings(bindings);

    this.#setLogMethods();
  }

  /**
   * Pre-serialize bindings
   * @param {object} bindings - Context fields to serialize
   * @returns {string} Pre-serialized bindings as JSON string
   * @private
   */
  #serializeBindings(bindings) {
    if (!bindings || typeof bindings !== 'object') {
      return '';
    }

    let result = '';
    const keys = ObjectKeys(bindings);
    for (const key of keys) {
      const serialized = this.#serializeValue(bindings[key], key);
      result += `,"${key}":${JSONStringify(serialized)}`;
    }
    return result;
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

    const mergedBindings = ObjectAssign(
      { __proto__: null },
      this.#bindings,
      bindings,
    );

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
      level: options.level || this.#level,
      bindings: mergedBindings,
      serializers: childSerializers || this.#serializers,
    });

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

    if (typeof msgOrObj === 'string') {
      if (fields !== undefined) {
        validateObject(fields, 'fields');
      }
    } else if (!isNativeError(msgOrObj)) {
      validateObject(msgOrObj, 'obj');
      validateString(msgOrObj.msg, 'obj.msg');
    }

    const channel = channels[level];
    if (!channel.hasSubscribers) {
      return;
    }

    let msg;
    let logFields;

    if (isNativeError(msgOrObj)) {
      msg = msgOrObj.message;
      // Use err serializer for Error objects
      const serializedErr = this.#serializers.err ?
        this.#serializers.err(msgOrObj) :
        this.#serializeError(msgOrObj);

      logFields = {
        err: serializedErr,
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

      // Special handling for err/error fields
      if (logFields.err && isNativeError(restFields.err)) {
        logFields.err = this.#serializers.err ?
          this.#serializers.err(restFields.err) :
          this.#serializeError(restFields.err);
      }

      if (logFields.error && isNativeError(restFields.error)) {
        logFields.error = this.#serializers.err ?
          this.#serializers.err(restFields.error) :
          this.#serializeError(restFields.error);
      }
    }

    const record = {
      level,
      msg,
      time: DateNow(),
      bindingsStr: this.#bindingsStr, // Pre-serialized bindings string
      fields: logFields,
    };

    channel.publish(record);
  }

  /**
   * Serialize Error object for logging (with recursive cause handling)
   * @param {object} err - Error object to serialize
   * @param {Set} [seen] - Set to track circular references
   * @returns {object|string} Serialized error object or '[Circular]'
   * @private
   */
  #serializeError(err, seen = new SafeSet()) {
    if (seen.has(err)) {
      return '[Circular]';
    }
    seen.add(err);

    const serialized = {
      message: err.message,
      name: err.name,
      stack: err.stack,
    };

    if (err.code !== undefined) {
      serialized.code = err.code;
    }

    if (err.cause !== undefined) {
      serialized.cause = isNativeError(err.cause) ?
        this.#serializeError(err.cause, seen) :
        err.cause;
    }

    // Include additional own error properties (avoid prototype chain)
    const keys = ObjectKeys(err);
    for (let i = 0; i < keys.length; i++) {
      const key = keys[i];
      if (serialized[key] === undefined) {
        serialized[key] = err[key];
      }
    }

    return serialized;
  }
}

module.exports = {
  __proto__: null,
  Logger,
  LogConsumer,
  JSONConsumer,
  LEVELS,
  channels,
  stdSerializers,
  serialize,
};
