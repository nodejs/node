'use strict';

const { convertToValidSignal, deprecate } = require('internal/util');
const {
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_ARG_TYPE,
  ERR_OUT_OF_RANGE
} = require('internal/errors').codes;
const { validateString, isInt32 } = require('internal/validators');

const _deprecatedCustomFds = deprecate(
  function deprecateCustomFds(options) {
    options.stdio = options.customFds.map(function mapCustomFds(fd) {
      return fd === -1 ? 'pipe' : fd;
    });
  }, 'child_process: options.customFds option is deprecated. ' +
     'Use options.stdio instead.', 'DEP0006');

function _convertCustomFds(options) {
  if (options.customFds && !options.stdio) {
    _deprecatedCustomFds(options);
  }
}

function validateTimeout(timeout) {
  if (timeout != null && !(Number.isInteger(timeout) && timeout >= 0)) {
    throw new ERR_OUT_OF_RANGE('timeout', 'an unsigned integer', timeout);
  }
}

function validateMaxBuffer(maxBuffer) {
  if (maxBuffer != null && !(typeof maxBuffer === 'number' && maxBuffer >= 0)) {
    throw new ERR_OUT_OF_RANGE('options.maxBuffer',
                               'a positive number',
                               maxBuffer);
  }
}

function sanitizeKillSignal(killSignal) {
  if (typeof killSignal === 'string' || typeof killSignal === 'number') {
    return convertToValidSignal(killSignal);
  } else if (killSignal != null) {
    throw new ERR_INVALID_ARG_TYPE('options.killSignal',
                                   ['string', 'number'],
                                   killSignal);
  }
}

function normalizeExecArgs(command, options, callback) {
  if (typeof options === 'function') {
    callback = options;
    options = undefined;
  }

  // Make a shallow copy so we don't clobber the user's options object.
  options = Object.assign({}, options);
  options.shell = typeof options.shell === 'string' ? options.shell : true;

  return {
    file: command,
    options: options,
    callback: callback
  };
}

function normalizeSpawnArguments(file, args, options) {
  validateString(file, 'file');

  if (file.length === 0)
    throw new ERR_INVALID_ARG_VALUE('file', file, 'cannot be empty');

  if (Array.isArray(args)) {
    args = args.slice(0);
  } else if (args !== undefined &&
             (args === null || typeof args !== 'object')) {
    throw new ERR_INVALID_ARG_TYPE('args', 'object', args);
  } else {
    options = args;
    args = [];
  }

  if (options === undefined)
    options = {};
  else if (options === null || typeof options !== 'object')
    throw new ERR_INVALID_ARG_TYPE('options', 'object', options);

  // Validate the cwd, if present.
  if (options.cwd != null &&
      typeof options.cwd !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('options.cwd', 'string', options.cwd);
  }

  // Validate detached, if present.
  if (options.detached != null &&
      typeof options.detached !== 'boolean') {
    throw new ERR_INVALID_ARG_TYPE('options.detached',
                                   'boolean', options.detached);
  }

  // Validate the uid, if present.
  if (options.uid != null && !isInt32(options.uid)) {
    throw new ERR_INVALID_ARG_TYPE('options.uid', 'int32', options.uid);
  }

  // Validate the gid, if present.
  if (options.gid != null && !isInt32(options.gid)) {
    throw new ERR_INVALID_ARG_TYPE('options.gid', 'int32', options.gid);
  }

  // Validate the shell, if present.
  if (options.shell != null &&
      typeof options.shell !== 'boolean' &&
      typeof options.shell !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('options.shell',
                                   ['boolean', 'string'], options.shell);
  }

  // Validate argv0, if present.
  if (options.argv0 != null &&
      typeof options.argv0 !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('options.argv0', 'string', options.argv0);
  }

  // Validate windowsHide, if present.
  if (options.windowsHide != null &&
      typeof options.windowsHide !== 'boolean') {
    throw new ERR_INVALID_ARG_TYPE('options.windowsHide',
                                   'boolean', options.windowsHide);
  }

  // Validate windowsVerbatimArguments, if present.
  if (options.windowsVerbatimArguments != null &&
      typeof options.windowsVerbatimArguments !== 'boolean') {
    throw new ERR_INVALID_ARG_TYPE('options.windowsVerbatimArguments',
                                   'boolean',
                                   options.windowsVerbatimArguments);
  }

  // Make a shallow copy so we don't clobber the user's options object.
  options = Object.assign({}, options);

  if (options.shell) {
    const command = [file].concat(args).join(' ');
    // Set the shell, switches, and commands.
    if (process.platform === 'win32') {
      if (typeof options.shell === 'string')
        file = options.shell;
      else
        file = process.env.comspec || 'cmd.exe';
      // '/d /s /c' is used only for cmd.exe.
      if (/^(?:.*\\)?cmd(?:\.exe)?$/i.test(file)) {
        args = ['/d', '/s', '/c', `"${command}"`];
        options.windowsVerbatimArguments = true;
      } else {
        args = ['-c', command];
      }
    } else {
      if (typeof options.shell === 'string')
        file = options.shell;
      else if (process.platform === 'android')
        file = '/system/bin/sh';
      else
        file = '/bin/sh';
      args = ['-c', command];
    }
  }

  if (typeof options.argv0 === 'string') {
    args.unshift(options.argv0);
  } else {
    args.unshift(file);
  }

  var env = options.env || process.env;
  var envPairs = [];

  // process.env.NODE_V8_COVERAGE always propagates, making it possible to
  // collect coverage for programs that spawn with white-listed environment.
  if (process.env.NODE_V8_COVERAGE &&
      !Object.prototype.hasOwnProperty.call(options.env || {},
                                            'NODE_V8_COVERAGE')) {
    env.NODE_V8_COVERAGE = process.env.NODE_V8_COVERAGE;
  }

  // Prototype values are intentionally included.
  for (var key in env) {
    const value = env[key];
    if (value !== undefined) {
      envPairs.push(`${key}=${value}`);
    }
  }

  _convertCustomFds(options);

  return {
    file: file,
    args: args,
    options: options,
    envPairs: envPairs
  };
}

module.exports = {
  normalizeExecArgs,
  normalizeSpawnArguments,
  sanitizeKillSignal,
  validateTimeout,
  validateMaxBuffer
};
