'use strict';

const {
  DateNow,
  JSONStringify,
  ObjectAssign,
  ObjectKeys,
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
   * Internal log method
   * @param {string} level
   * @param {number} levelValue
   * @param {object} obj
   */
  _log(level, levelValue, obj) {
    // Note: Level check is now done at method assignment time (noop)
    // So this function is only called for enabled levels

    // Validate required msg field
    validateObject(obj, 'obj');
    if (typeof obj.msg !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('obj.msg', 'string', obj.msg);
    }

    // Extract msg and remaining fields
    const { msg, ...fields } = obj;

    // Build log record
    const record = {
      level,
      msg,
      time: DateNow(),
      bindings: this._bindings,
      fields,
    };

    this._handler.handle(record);
  }

  /**
   * Log at trace level
   * @param {object} obj - Object with required msg property
   */
  trace(obj) {
    this._log('trace', 10, obj);
  }

  /**
   * Log at debug level
   * @param {object} obj - Object with required msg property
   */
  debug(obj) {
    this._log('debug', 20, obj);
  }

  /**
   * Log at info level
   * @param {object} obj - Object with required msg property
   */
  info(obj) {
    this._log('info', 30, obj);
  }

  /**
   * Log at warn level
   * @param {object} obj - Object with required msg property
   */
  warn(obj) {
    this._log('warn', 40, obj);
  }

  /**
   * Log at error level
   * @param {object} obj - Object with required msg property
   */
  error(obj) {
    this._log('error', 50, obj);
  }

  /**
   * Log at fatal level (does NOT exit the process)
   * @param {object} obj - Object with required msg property
   */
  fatal(obj) {
    this._log('fatal', 60, obj);
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
