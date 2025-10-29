'use strict';

const {
  DateNow,
  Error,
  JSONStringify,
  ObjectAssign,
  ObjectKeys,
  ObjectPrototypeToString,
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
} = require('internal/validators');

const Utf8Stream = require('internal/streams/fast-utf8-stream');

// RFC5424 numerical ordering + log4j interface
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

class Handler {
  constructor(options = {}) {
    validateObject(options, 'options');
    const { level = 'info' } = options;
    validateString(level, 'options.level');

    if (!LEVELS[level]) {
      throw new ERR_INVALID_ARG_VALUE('options.level', level,
                                      `must be one of: ${LEVEL_NAMES.join(', ')}`);
    }

    this._level = level;
    this._levelValue = LEVELS[level]; // Cache numeric value
  }

  /**
   * Check if a level would be logged
   * @param {string} level
   * @returns {boolean}
   */
  enabled(level) {
    return LEVELS[level] >= this._levelValue;
  }

  /**
   * Handle a log record
   * @param {object} record
   */
  handle(record) {
    throw new ERR_METHOD_NOT_IMPLEMENTED('Handler.handle()');
  }
}

/**
 * JSON handler - outputs structured JSON logs
 */
class JSONHandler extends Handler {
  constructor(options = {}) {
    super(options);
    const {
      stream,
      fields = {},
    } = options;

    validateObject(fields, 'options.fields');

    // Default to stdout
    this._stream = stream ? this._createStream(stream) :
      new Utf8Stream({ fd: 1 });
    this._fields = fields;
  }

  _createStream(stream) {
    // If it's already a Utf8Stream, use it directly
    if (stream instanceof Utf8Stream) {
      return stream;
    }

    // If it's a file descriptor number
    if (typeof stream === 'number') {
      return new Utf8Stream({ fd: stream });
    }

    // If it's a path string
    if (typeof stream === 'string') {
      return new Utf8Stream({ dest: stream });
    }

    throw new ERR_INVALID_ARG_TYPE('options.stream',
                                   ['number', 'string', 'Utf8Stream'],
                                   stream);
  }

  handle(record) {
    // Note: Level check already done in Logger._log()
    // No need to check again here

    // Build the JSON log record
    const logObj = {
      level: record.level,
      time: record.time,
      msg: record.msg,
      ...this._fields,      // Additional fields (hostname, pid, etc)
      ...record.bindings,   // Parent context
      ...record.fields,     // Log-specific fields
    };

    const json = JSONStringify(logObj) + '\n';
    this._stream.write(json);
  }

  /**
   * Flush pending writes
   * @param {Function} callback
   */
  flush(callback) {
    this._stream.flush(callback);
  }

  /**
   * Flush pending writes synchronously
   */
  flushSync() {
    this._stream.flushSync();
  }

  /**
   * Close the handler
   */
  end() {
    this._stream.end();
  }
}

/**
 * Logger class
 */
class Logger {
  constructor(options = {}) {
    validateObject(options, 'options');
    const {
      handler = new JSONHandler(),
      level,
      bindings = {},
    } = options;

    if (!(handler instanceof Handler)) {
      throw new ERR_INVALID_ARG_TYPE('options.handler', 'Handler', handler);
    }

    validateObject(bindings, 'options.bindings');

    this._handler = handler;
    this._bindings = bindings;

    // If level is specified, it overrides handler's level
    if (level !== undefined) {
      validateString(level, 'options.level');
      if (!LEVELS[level]) {
        throw new ERR_INVALID_ARG_VALUE('options.level', level,
                                        `must be one of: ${LEVEL_NAMES.join(', ')}`);
      }
      this._level = level;
      this._levelValue = LEVELS[level]; // Cache numeric value
    } else {
      this._level = handler._level;
      this._levelValue = handler._levelValue; // Use handler's cached value
    }

    // Optimize: replace disabled log methods with noop
    this._setLogMethods();
  }

  /**
   * Replace disabled log methods with noop for performance
   * @private
   */
  _setLogMethods() {
    const levelValue = this._levelValue;

    // Override instance methods for disabled levels
    // This avoids the level check on every call
    if (levelValue > 10) this.trace = noop;
    if (levelValue > 20) this.debug = noop;
    if (levelValue > 30) this.info = noop;
    if (levelValue > 40) this.warn = noop;
    if (levelValue > 50) this.error = noop;
    if (levelValue > 60) this.fatal = noop;
  }

  /**
   * Check if a level would be logged
   * @param {string} level
   * @returns {boolean}
   */
  enabled(level) {
    return LEVELS[level] >= this._levelValue;
  }

  /**
   * Create a child logger with additional context
   * @param {object} bindings - Context to add to all child logs
   * @param {object} [options] - Optional overrides
   * @returns {Logger}
   */
  child(bindings, options = {}) {
    validateObject(bindings, 'bindings');
    validateObject(options, 'options');

    // Shallow merge parent and child bindings
    const mergedBindings = ObjectAssign(
      { __proto__: null },
      this._bindings,
      bindings,
    );

    // Create new logger inheriting handler
    return new Logger({
      handler: this._handler,
      level: options.level || this._level,
      bindings: mergedBindings,
    });
  }

  /**
   * Check if value is an Error object
   * @param {*} value
   * @returns {boolean}
   * @private
   */
  _isError(value) {
    return value !== null &&
           typeof value === 'object' &&
           (ObjectPrototypeToString(value) === '[object Error]' ||
            value instanceof Error);
  }

  /**
   * Internal log method
   * @param {string} level
   * @param {number} levelValue
   * @param {string|object} msgOrObj - Message string or object with msg property
   * @param {object} [fields] - Optional fields to merge
   */
  _log(level, levelValue, msgOrObj, fields) {
    let msg;
    let logFields;

    // Support two signatures:
    // 1. logger.info('message', { fields })
    // 2. logger.info({ msg: 'message', other: 'fields' })
    // 3. logger.info(new Error('boom'))

    if (this._isError(msgOrObj)) {
      // Handle Error as first argument
      msg = msgOrObj.message;
      logFields = {
        err: this._serializeError(msgOrObj),
        ...fields,
      };
    } else if (typeof msgOrObj === 'string') {
      // Support String message
      msg = msgOrObj;
      logFields = fields || {};
      validateObject(logFields, 'fields');
    } else {
      // Support object with msg property
      validateObject(msgOrObj, 'obj');
      if (typeof msgOrObj.msg !== 'string') {
        throw new ERR_INVALID_ARG_TYPE('obj.msg', 'string', msgOrObj.msg);
      }
      const { msg: extractedMsg, ...restFields } = msgOrObj;
      msg = extractedMsg;
      logFields = restFields;

      // Serialize Error objects in fields
      if (logFields.err && this._isError(logFields.err)) {
        logFields.err = this._serializeError(logFields.err);
      }
    }

    // Build log record
    const record = {
      level,
      msg,
      time: DateNow(),
      bindings: this._bindings,
      fields: logFields,
    };

    this._handler.handle(record);
  }

  /**
   * Serialize Error object for logging
   * @param {object} err - Error object to serialize
   * @returns {object} Serialized error object
   * @private
   */
  _serializeError(err) {
    const serialized = {
      message: err.message,
      name: err.name,
      stack: err.stack,
    };

    // Add code if it exists
    serialized.code ||= err.code;

    // Add any enumerable custom properties
    for (const key in err) {
      serialized[key] ||= err[key];
    }

    return serialized;
  }

  /**
   * Log at trace level
   * @param {string|object} msgOrObj - Message string or object with msg property
   * @param {object} [fields] - Optional fields to merge
   */
  trace(msgOrObj, fields) {
    this._log('trace', 10, msgOrObj, fields);
  }

  /**
   * Log at debug level
   * @param {string|object} msgOrObj - Message string or object with msg property
   * @param {object} [fields] - Optional fields to merge
   */
  debug(msgOrObj, fields) {
    this._log('debug', 20, msgOrObj, fields);
  }

  /**
   * Log at info level
   * @param {string|object} msgOrObj - Message string or object with msg property
   * @param {object} [fields] - Optional fields to merge
   */
  info(msgOrObj, fields) {
    this._log('info', 30, msgOrObj, fields);
  }

  /**
   * Log at warn level
   * @param {string|object} msgOrObj - Message string or object with msg property
   * @param {object} [fields] - Optional fields to merge
   */
  warn(msgOrObj, fields) {
    this._log('warn', 40, msgOrObj, fields);
  }

  /**
   * Log at error level
   * @param {string|object} msgOrObj - Message string or object with msg property
   * @param {object} [fields] - Optional fields to merge
   */
  error(msgOrObj, fields) {
    this._log('error', 50, msgOrObj, fields);
  }

  /**
   * Log at fatal level (does NOT exit the process)
   * @param {string|object} msgOrObj - Message string or object with msg property
   * @param {object} [fields] - Optional fields to merge
   */
  fatal(msgOrObj, fields) {
    this._log('fatal', 60, msgOrObj, fields);
  }

  /**
   * Flush pending writes
   * @param {Function} callback
   */
  flush(callback) {
    this._handler.flush(callback);
  }
}

/**
 * Create a new logger instance
 * @param {object} [options]
 * @param {Handler} [options.handler] - Output handler (default: JSONHandler)
 * @param {string} [options.level] - Minimum log level (default: 'info')
 * @returns {Logger}
 */
function createLogger(options) {
  return new Logger(options);
}

module.exports = {
  createLogger,
  Logger,
  Handler,
  JSONHandler,
  LEVELS,
};
