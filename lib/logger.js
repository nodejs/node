'use strict';

const {
  DateNow,
  Error,
  JSONStringify,
  ObjectAssign,
  ObjectKeys,
  SafeSet,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_METHOD_NOT_IMPLEMENTED,
  },
} = require('internal/errors');

const {
  validateObject,
  validateString,
  validateOneOf,
} = require('internal/validators');

const Utf8Stream = require('internal/streams/fast-utf8-stream');
const diagnosticsChannel = require('diagnostics_channel');
const { kEmptyObject } = require('internal/util');

// Create channels for each log level
const channels = {
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
  #levelValue;

  constructor(options = kEmptyObject) {
    validateObject(options, 'options');
    const { level = 'info' } = options;
    validateString(level, 'options.level');

    validateOneOf(level, 'options.level', LEVEL_NAMES);

    this.#level = level;
    this.#levelValue = LEVELS[level];
  }

  /**
   * Check if a level would be consumed
   * @param {string} level
   * @returns {boolean}
   */
  enabled(level) {
    return LEVELS[level] >= this.#levelValue;
  }

  /**
   * Attach this consumer to log channels
   */
  attach() {
    for (const level of LEVEL_NAMES) {
      if (this.enabled(level)) {
        channels[level].subscribe(this.#handleLog.bind(this, level));
      }
    }
  }

  /**
   * Internal handler called by diagnostics_channel
   * @param {string} level
   * @param {object} record
   * @private
   */
  #handleLog(level, record) {
    this.handle(record);
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
    const logObj = {
      level: record.level,
      time: record.time,
      msg: record.msg,
    };

    // Merge fields in order: consumer fields -> bindings -> log fields
    // Using direct property assignment for better performance
    const consumerFields = this.#fields;
    for (const key in consumerFields) {
      logObj[key] = consumerFields[key];
    }

    const bindings = record.bindings;
    for (const key in bindings) {
      logObj[key] = bindings[key];
    }

    const fields = record.fields;
    for (const key in fields) {
      logObj[key] = fields[key];
    }

    const json = JSONStringify(logObj) + '\n';
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

  constructor(options = kEmptyObject) {
    validateObject(options, 'options');
    const { level = 'info', bindings = kEmptyObject } = options;

    validateString(level, 'options.level');
    if (!LEVELS[level]) {
      throw new ERR_INVALID_ARG_VALUE(
        'options.level',
        level,
        `must be one of: ${LEVEL_NAMES.join(', ')}`,
      );
    }

    validateObject(bindings, 'options.bindings');

    this.#level = level;
    this.#levelValue = LEVELS[level];
    this.#bindings = bindings;

    this.#setLogMethods();
  }

  /**
   * Replace disabled log methods with noop for performance
   * @private
   */
  #setLogMethods() {
    const levelValue = this.#levelValue;

    if (levelValue > LEVELS.trace) this.trace = noop;
    if (levelValue > LEVELS.debug) this.debug = noop;
    if (levelValue > LEVELS.info) this.info = noop;
    if (levelValue > LEVELS.warn) this.warn = noop;
    if (levelValue > LEVELS.error) this.error = noop;
    if (levelValue > LEVELS.fatal) this.fatal = noop;
  }

  /**
   * Check if a level would be logged
   * @param {string} level
   * @returns {boolean}
   */
  enabled(level) {
    return LEVELS[level] >= this.#levelValue;
  }

  /**
   * Create a child logger with additional context
   * @param {object} bindings - Context to add to all child logs
   * @param {object} [options] - Optional overrides
   * @returns {Logger}
   */
  child(bindings, options = kEmptyObject) {
    validateObject(bindings, 'bindings');
    validateObject(options, 'options');

    const mergedBindings = ObjectAssign(
      { __proto__: null },
      this.#bindings,
      bindings,
    );

    const childLogger = new Logger({
      level: options.level || this.#level,
      bindings: mergedBindings,
    });

    return childLogger;
  }

  /**
   * Check if value is an Error object
   * @param {*} value
   * @returns {boolean}
   * @private
   */
  #isError(value) {
    return Error.isError(value);
  }

  /**
   * Internal log method
   * @param {string} level
   * @param {number} levelValue
   * @param {string|object} msgOrObj - Message string or object with msg property
   * @param {object} [fields] - Optional fields to merge
   */
  #log(level, levelValue, msgOrObj, fields) {
    if (levelValue < this.#levelValue) {
      return;
    }

    if (typeof msgOrObj === 'string') {
      if (fields !== undefined) {
        validateObject(fields, 'fields');
      }
    } else if (!this.#isError(msgOrObj)) {
      validateObject(msgOrObj, 'obj');
      validateString(msgOrObj.msg, 'obj.msg');
    }

    const channel = channels[level];
    if (!channel.hasSubscribers) {
      return;
    }

    let msg;
    let logFields;

    if (this.#isError(msgOrObj)) {
      msg = msgOrObj.message;
      logFields = {
        err: this.#serializeError(msgOrObj),
        ...fields,
      };
    } else if (typeof msgOrObj === 'string') {
      msg = msgOrObj;
      logFields = fields ?? kEmptyObject;
    } else {
      const { msg: extractedMsg, ...restFields } = msgOrObj;
      msg = extractedMsg;
      logFields = restFields;

      if (logFields.err && this.#isError(logFields.err)) {
        logFields.err = this.#serializeError(logFields.err);
      }

      if (logFields.error && this.#isError(logFields.error)) {
        logFields.error = this.#serializeError(logFields.error);
      }
    }

    const record = {
      level,
      msg,
      time: DateNow(),
      bindings: this.#bindings,
      fields: logFields,
    };

    channel.publish(record);
  }

  /**
   * Serialize Error object for logging (with recursive cause handling)
   * @param {object} err - Error object to serialize
   * @param {Set} [seen] - Set to track circular references
   * @returns {object} Serialized error object
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
      serialized.cause = this.#isError(err.cause) ?
        this.#serializeError(err.cause, seen) :
        err.cause;
    }

    for (const key in err) {
      if (serialized[key] === undefined) {
        serialized[key] = err[key];
      }
    }

    return serialized;
  }

  trace(msgOrObj, fields) {
    this.#log('trace', LEVELS.trace, msgOrObj, fields);
  }

  debug(msgOrObj, fields) {
    this.#log('debug', LEVELS.debug, msgOrObj, fields);
  }

  info(msgOrObj, fields) {
    this.#log('info', LEVELS.info, msgOrObj, fields);
  }

  warn(msgOrObj, fields) {
    this.#log('warn', LEVELS.warn, msgOrObj, fields);
  }

  error(msgOrObj, fields) {
    this.#log('error', LEVELS.error, msgOrObj, fields);
  }

  fatal(msgOrObj, fields) {
    this.#log('fatal', LEVELS.fatal, msgOrObj, fields);
  }
}

/**
 * Create a logger instance (convenience method)
 * @param {object} [options]
 * @param {string} [options.level] - Minimum log level (default: 'info')
 * @param {object} [options.bindings] - Context fields (default: {})
 * @returns {Logger}
 */
function createLogger(options) {
  return new Logger(options);
}

module.exports = {
  __proto__: null,
  Logger,
  LogConsumer,
  JSONConsumer,
  LEVELS,
  channels,
  createLogger,
  Handler: LogConsumer,
  JSONHandler: JSONConsumer,
};
