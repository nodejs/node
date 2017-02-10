'use strict';

const config = process.binding('config');
const prefix = `(${process.release.name}:${process.pid}) `;

exports.setup = setupProcessWarnings;

var errors;
var fs;
var cachedFd;
var acquiringFd = false;
function nop() {}

function lazyErrors() {
  if (!errors)
    errors = require('internal/errors');
  return errors;
}

function lazyFs() {
  if (!fs)
    fs = require('fs');
  return fs;
}

function writeOut(message) {
  if (console && typeof console.error === 'function')
    return console.error(message);
  process._rawDebug(message);
}

function onClose(fd) {
  return function() {
    lazyFs().close(fd, nop);
  };
}

function onOpen(cb) {
  return function(err, fd) {
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
  // make a best effort attempt at writing the message
  // to the fd. Errors are ignored at this point.
  return function(err, fd) {
    if (err)
      return writeOut(message);
    lazyFs().appendFile(fd, `${message}\n`, nop);
  };
}

function acquireFd(cb) {
  if (cachedFd === undefined && !acquiringFd) {
    acquiringFd = true;
    lazyFs().open(config.warningFile, 'a', onOpen(cb));
  } else if (cachedFd !== undefined && !acquiringFd) {
    cb(null, cachedFd);
  } else {
    process.once('_node_warning_fd_acquired', cb);
  }
}

function output(message) {
  if (typeof config.warningFile === 'string') {
    acquireFd(onAcquired(message));
    return;
  }
  writeOut(message);
}

function doEmitWarning(warning) {
  return function() {
    process.emit('warning', warning);
  };
}

function setupProcessWarnings() {
  if (!process.noProcessWarnings && process.env.NODE_NO_WARNINGS !== '1') {
    process.on('warning', (warning) => {
      if (!(warning instanceof Error)) return;
      const isDeprecation = warning.name === 'DeprecationWarning';
      if (isDeprecation && process.noDeprecation) return;
      const trace = process.traceProcessWarnings ||
                    (isDeprecation && process.traceDeprecation);
      if (trace && warning.stack) {
        if (warning.code) {
          output(`${prefix}[${warning.code}] ${warning.stack}`);
        } else {
          output(`${prefix}${warning.stack}`);
        }
      } else {
        const toString =
          typeof warning.toString === 'function' ?
            warning.toString : Error.prototype.toString;
        if (warning.code) {
          output(`${prefix}[${warning.code}] ${toString.apply(warning)}`);
        } else {
          output(`${prefix}${toString.apply(warning)}`);
        }
      }
    });
  }

  // process.emitWarning(error)
  // process.emitWarning(str[, type[, code]][, ctor])
  process.emitWarning = function(warning, type, code, ctor) {
    const errors = lazyErrors();
    if (typeof type === 'function') {
      ctor = type;
      code = undefined;
      type = 'Warning';
    }
    if (typeof code === 'function') {
      ctor = code;
      code = undefined;
    }
    if (code !== undefined && typeof code !== 'string')
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'code', 'string');
    if (type !== undefined && typeof type !== 'string')
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'type', 'string');
    if (warning === undefined || typeof warning === 'string') {
      warning = new Error(warning);
      warning.name = String(type || 'Warning');
      if (code !== undefined) warning.code = code;
      Error.captureStackTrace(warning, ctor || process.emitWarning);
    }
    if (!(warning instanceof Error)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                                 'warning', ['Error', 'string']);
    }
    if (warning.name === 'DeprecationWarning') {
      if (process.noDeprecation)
        return;
      if (process.throwDeprecation)
        throw warning;
    }
    process.nextTick(doEmitWarning(warning));
  };
}
