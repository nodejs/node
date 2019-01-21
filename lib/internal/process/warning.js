'use strict';

const prefix = `(${process.release.name}:${process.pid}) `;
const { ERR_INVALID_ARG_TYPE } = require('internal/errors').codes;

let options;
function lazyOption(name) {
  if (!options) {
    options = require('internal/options');
  }
  return options.getOptionValue(name);
}

var cachedFd;
var acquiringFd = false;
function nop() {}

// Lazily loaded
var fs = null;

function writeOut(message) {
  if (console && typeof console.error === 'function')
    return console.error(message);
  process._rawDebug(message);
}

function onClose(fd) {
  return () => {
    if (fs === null) fs = require('fs');
    try {
      fs.closeSync(fd);
    } catch {}
  };
}

function onOpen(cb) {
  return (err, fd) => {
    acquiringFd = false;
    if (fd !== undefined) {
      cachedFd = fd;
      process.on('exit', onClose(fd));
    }
    cb(err, fd);
    process.emit('_node_warning_fd_acquired', err, fd);
  };
}

function onAcquired(message) {
  // Make a best effort attempt at writing the message
  // to the fd. Errors are ignored at this point.
  return (err, fd) => {
    if (err)
      return writeOut(message);
    if (fs === null) fs = require('fs');
    fs.appendFile(fd, `${message}\n`, nop);
  };
}

function acquireFd(warningFile, cb) {
  if (cachedFd === undefined && !acquiringFd) {
    acquiringFd = true;
    if (fs === null) fs = require('fs');
    fs.open(warningFile, 'a', onOpen(cb));
  } else if (cachedFd !== undefined && !acquiringFd) {
    cb(null, cachedFd);
  } else {
    process.once('_node_warning_fd_acquired', cb);
  }
}

function output(message) {
  const warningFile = lazyOption('--redirect-warnings');
  if (warningFile) {
    acquireFd(warningFile, onAcquired(message));
    return;
  }
  writeOut(message);
}

function doEmitWarning(warning) {
  return () => {
    process.emit('warning', warning);
  };
}

function onWarning(warning) {
  if (!(warning instanceof Error)) return;
  const isDeprecation = warning.name === 'DeprecationWarning';
  if (isDeprecation && process.noDeprecation) return;
  const trace = process.traceProcessWarnings ||
                (isDeprecation && process.traceDeprecation);
  var msg = prefix;
  if (warning.code)
    msg += `[${warning.code}] `;
  if (trace && warning.stack) {
    msg += `${warning.stack}`;
  } else {
    const toString =
      typeof warning.toString === 'function' ?
        warning.toString : Error.prototype.toString;
    msg += `${toString.apply(warning)}`;
  }
  if (typeof warning.detail === 'string') {
    msg += `\n${warning.detail}`;
  }
  output(msg);
}

// process.emitWarning(error)
// process.emitWarning(str[, type[, code]][, ctor])
// process.emitWarning(str[, options])
function emitWarning(warning, type, code, ctor, now) {
  let detail;
  if (type !== null && typeof type === 'object' && !Array.isArray(type)) {
    ctor = type.ctor;
    code = type.code;
    if (typeof type.detail === 'string')
      detail = type.detail;
    type = type.type || 'Warning';
  } else if (typeof type === 'function') {
    ctor = type;
    code = undefined;
    type = 'Warning';
  }
  if (type !== undefined && typeof type !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('type', 'string', type);
  }
  if (typeof code === 'function') {
    ctor = code;
    code = undefined;
  } else if (code !== undefined && typeof code !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('code', 'string', code);
  }
  if (typeof warning === 'string') {
    // Improve error creation performance by skipping the error frames.
    // They are added in the `captureStackTrace()` function below.
    const tmpStackLimit = Error.stackTraceLimit;
    Error.stackTraceLimit = 0;
    // eslint-disable-next-line no-restricted-syntax
    warning = new Error(warning);
    Error.stackTraceLimit = tmpStackLimit;
    warning.name = String(type || 'Warning');
    if (code !== undefined) warning.code = code;
    if (detail !== undefined) warning.detail = detail;
    Error.captureStackTrace(warning, ctor || process.emitWarning);
  } else if (!(warning instanceof Error)) {
    throw new ERR_INVALID_ARG_TYPE('warning', ['Error', 'string'], warning);
  }
  if (warning.name === 'DeprecationWarning') {
    if (process.noDeprecation)
      return;
    if (process.throwDeprecation)
      throw warning;
  }
  if (now) process.emit('warning', warning);
  else process.nextTick(doEmitWarning(warning));
}

module.exports = {
  onWarning,
  emitWarning
};
