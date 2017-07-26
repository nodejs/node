'use strict';

const traceWarnings = process.traceProcessWarnings;
const noDeprecation = process.noDeprecation;
const traceDeprecation = process.traceDeprecation;
const throwDeprecation = process.throwDeprecation;
const config = process.binding('config');
const prefix = `(${process.release.name}:${process.pid}) `;

exports.setup = setupProcessWarnings;

var fs;
var cachedFd;
var acquiringFd = false;
function nop() {}

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
      if (isDeprecation && noDeprecation) return;
      const trace = traceWarnings || (isDeprecation && traceDeprecation);
      if (trace && warning.stack) {
        console.error(`${prefix}${warning.stack}`);
      } else {
        var toString = warning.toString;
        if (typeof toString !== 'function')
          toString = Error.prototype.toString;
        output(`${prefix}${toString.apply(warning)}`);
      }
    });
  }

  // process.emitWarning(error)
  // process.emitWarning(str[, name][, ctor])
  process.emitWarning = function(warning, name, ctor) {
    if (typeof name === 'function') {
      ctor = name;
      name = 'Warning';
    }
    if (warning === undefined || typeof warning === 'string') {
      warning = new Error(warning);
      warning.name = name || 'Warning';
      Error.captureStackTrace(warning, ctor || process.emitWarning);
    }
    if (!(warning instanceof Error)) {
      throw new TypeError('\'warning\' must be an Error object or string.');
    }
    if (throwDeprecation && warning.name === 'DeprecationWarning')
      throw warning;
    else
      process.nextTick(doEmitWarning(warning));
  };
}
