'use strict';

const {
  ArrayIsArray,
  Error,
} = primordials;

const assert = require('internal/assert');
const { ERR_INVALID_ARG_TYPE } = require('internal/errors').codes;

// Lazily loaded
let fs;
let fd;
let warningFile;

function lazyOption() {
  // This will load `warningFile` only once. If the flag is not set,
  // `warningFile` will be set to an empty string.
  if (warningFile === undefined) {
    warningFile = require('internal/options')
                  .getOptionValue('--redirect-warnings');
  }
  return warningFile;
}

// If we can't write to stderr, we'd like to make this a noop,
// so use console.error.
let error;
function writeOut(message) {
  if (!error) {
    error = require('internal/console/global').error;
  }
  error(message);
}

function writeToFile(message) {
  if (fd === undefined) {
    fs = require('fs');
    try {
      fd = fs.openSync(warningFile, 'a');
    } catch {
      return writeOut(message);
    }
    process.on('exit', () => {
      try {
        fs.closeSync(fd);
      } catch {}
    });
  }
  fs.appendFile(fd, `${message}\n`, (err) => {
    if (err) {
      writeOut(message);
    }
  });
}

function doEmitWarning(warning) {
  return () => process.emit('warning', warning);
}

let traceWarningHelperShown = false;
function onWarning(warning) {
  if (!(warning instanceof Error)) return;
  const isDeprecation = warning.name === 'DeprecationWarning';
  if (isDeprecation && process.noDeprecation) return;
  const trace = process.traceProcessWarnings ||
                (isDeprecation && process.traceDeprecation);
  let msg = `(${process.release.name}:${process.pid}) `;
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
  if (!trace && !traceWarningHelperShown) {
    const flag = isDeprecation ? '--trace-deprecation' : '--trace-warnings';
    const argv0 = require('path').basename(process.argv0 || 'node', '.exe');
    msg += `\n(Use \`${argv0} ${flag} ...\` to show where the warning ` +
           'was created)';
    traceWarningHelperShown = true;
  }
  const warningFile = lazyOption();
  if (warningFile) {
    return writeToFile(msg);
  }
  writeOut(msg);
}

// process.emitWarning(error)
// process.emitWarning(str[, type[, code]][, ctor])
// process.emitWarning(str[, options])
function emitWarning(warning, type, code, ctor) {
  let detail;
  if (type !== null && typeof type === 'object' && !ArrayIsArray(type)) {
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
    warning = createWarningObject(warning, type, code, ctor, detail);
  } else if (!(warning instanceof Error)) {
    throw new ERR_INVALID_ARG_TYPE('warning', ['Error', 'string'], warning);
  }
  if (warning.name === 'DeprecationWarning') {
    if (process.noDeprecation)
      return;
    if (process.throwDeprecation)
      throw warning;
  }
  process.nextTick(doEmitWarning(warning));
}

function emitWarningSync(warning) {
  process.emit('warning', createWarningObject(warning));
}

function createWarningObject(warning, type, code, ctor, detail) {
  assert(typeof warning === 'string');
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
  // eslint-disable-next-line no-restricted-syntax
  Error.captureStackTrace(warning, ctor || process.emitWarning);
  return warning;
}

module.exports = {
  emitWarning,
  emitWarningSync,
  onWarning,
};
