'use strict';

const prefix = `(${process.release.name}:${process.pid}) `;

exports.setup = setupProcessWarnings;

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
          console.error(`${prefix}[${warning.code}] ${warning.stack}`);
        } else {
          console.error(`${prefix}${warning.stack}`);
        }
      } else {
        const toString =
          typeof warning.toString === 'function' ?
            warning.toString : Error.prototype.toString;
        if (warning.code) {
          console.error(
              `${prefix}[${warning.code}] ${toString.apply(warning)}`);
        } else {
          console.error(`${prefix}${toString.apply(warning)}`);
        }
      }
    });
  }

  // process.emitWarning(error)
  // process.emitWarning(str[, type[, code]][, ctor])
  process.emitWarning = function(warning, type, code, ctor) {
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
      throw new TypeError('\'code\' must be a String');
    if (type !== undefined && typeof type !== 'string')
      throw new TypeError('\'type\' must be a String');
    if (warning === undefined || typeof warning === 'string') {
      warning = new Error(warning);
      warning.name = String(type || 'Warning');
      if (code !== undefined) warning.code = code;
      Error.captureStackTrace(warning, ctor || process.emitWarning);
    }
    if (!(warning instanceof Error)) {
      throw new TypeError('\'warning\' must be an Error object or string.');
    }
    if (warning.name === 'DeprecationWarning') {
      if (process.noDeprecation)
        return;
      if (process.throwDeprecation)
        throw warning;
    }
    process.nextTick(() => process.emit('warning', warning));
  };
}
