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
const diagnosticsChannel = require('diagnostics_channel');

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
  constructor(options = kEmptyObject) {
    validateObject(options, 'options');
    const { level = 'info' } = options;
    validateString(level, 'options.level');

    if (!LEVELS[level]) {
      throw new ERR_INVALID_ARG_VALUE('options.level', level,
                                      `must be one of: ${LEVEL_NAMES.join(', ')}`);
    }

    this._level = level;
    this._levelValue = LEVELS[level];
  }

  /**
   * Check if a level would be consumed
   * @param {string} level
   * @returns {boolean}
   */
  enabled(level) {
    return LEVELS[level] >= this._levelValue;
  }

  /**
   * Attach this consumer to log channels
   */
  attach() {
    for (const level of LEVEL_NAMES) {
      if (this.enabled(level)) {
        channels[level].subscribe(this._handleLog.bind(this, level));
      }
    }
  }

  /**
   * Internal handler called by diagnostics_channel
   * @param {string} level
   * @param {object} record
   * @private
   */
  _handleLog(level, record) {
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
  constructor(options = kEmptyObject) {
    super(options);
    const {
      stream,
      fields = kEmptyObject,
    } = options;

    validateObject(fields, 'options.fields');

    this._stream = stream ? this._createStream(stream) :
      new Utf8Stream({ fd: 1 });
    this._fields = fields;
  }

  _createStream(stream) {
    if (stream instanceof Utf8Stream) {
      return stream;
    }

    if (typeof stream === 'number') {
      return new Utf8Stream({ fd: stream });
    }

    if (typeof stream === 'string') {
      return new Utf8Stream({ dest: stream });
    }

    throw new ERR_INVALID_ARG_TYPE('options.stream',
                                   ['number', 'string', 'Utf8Stream'],
                                   stream);
  }

  handle(record) {
    const logObj = {
      level: record.level,
      time: record.time,
      msg: record.msg,
      ...this._fields,
      ...record.bindings,
      ...record.fields,
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
   * Close the consumer
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
      level = 'info',
      bindings = {},
    } = options;

    validateString(level, 'options.level');
    if (!LEVELS[level]) {
      throw new ERR_INVALID_ARG_VALUE('options.level', level,
                                      `must be one of: ${LEVEL_NAMES.join(', ')}`);
    }

    validateObject(bindings, 'options.bindings');

    this._level = level;
    this._levelValue = LEVELS[level];
    this._bindings = bindings;

    // Optimize: replace disabled log methods with noop
    this._setLogMethods();
  }

  /**
   * Replace disabled log methods with noop for performance
   * @private
   */
  _setLogMethods() {
    const levelValue = this._levelValue;

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

    const mergedBindings = ObjectAssign(
      { __proto__: null },
      this._bindings,
      bindings,
    );

    return new Logger({
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
    if (levelValue < this._levelValue) {
      return;
    }

    let msg;
    let logFields;

    if (this._isError(msgOrObj)) {
      msg = msgOrObj.message;
      logFields = {
        err: this._serializeError(msgOrObj),
        ...fields,
      };
    } else if (typeof msgOrObj === 'string') {
      msg = msgOrObj;
      logFields = fields || {};
      validateObject(logFields, 'fields');
    } else {
      validateObject(msgOrObj, 'obj');
      if (typeof msgOrObj.msg !== 'string') {
        throw new ERR_INVALID_ARG_TYPE('obj.msg', 'string', msgOrObj.msg);
      }
      const { msg: extractedMsg, ...restFields } = msgOrObj;
      msg = extractedMsg;
      logFields = restFields;

      if (logFields.err && this._isError(logFields.err)) {
        logFields.err = this._serializeError(logFields.err);
      }
    }

    const record = {
      level,
      msg,
      time: DateNow(),
      bindings: this._bindings,
      fields: logFields,
    };

    const channel = channels[level];
    if (channel.hasSubscribers) {
      channel.publish(record);
    }
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

    serialized.code ||= err.code;

    for (const key in err) {
      serialized[key] ||= err[key];
    }

    return serialized;
  }

  trace(msgOrObj, fields) {
    this._log('trace', 10, msgOrObj, fields);
  }

  debug(msgOrObj, fields) {
    this._log('debug', 20, msgOrObj, fields);
  }

  info(msgOrObj, fields) {
    this._log('info', 30, msgOrObj, fields);
  }

  warn(msgOrObj, fields) {
    this._log('warn', 40, msgOrObj, fields);
  }

  error(msgOrObj, fields) {
    this._log('error', 50, msgOrObj, fields);
  }

  fatal(msgOrObj, fields) {
    this._log('fatal', 60, msgOrObj, fields);
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
  Logger,
  LogConsumer,
  JSONConsumer,
  LEVELS,
  channels,
  createLogger,
  Handler: LogConsumer,
  JSONHandler: JSONConsumer,
};
