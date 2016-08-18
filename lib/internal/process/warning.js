'use strict';

const prefix = `(${process.release.name}:${process.pid}) `;

exports.setup = setupProcessWarnings;

function setupProcessWarnings() {
  if (!process.noProcessWarnings) {
    process.on('warning', (warning) => {
      if (!(warning instanceof Error)) return;
      const isDeprecation = warning.name === 'DeprecationWarning';
      if (isDeprecation && process.noDeprecation) return;
      const trace = process.traceProcessWarnings ||
                    (isDeprecation && process.traceDeprecation);
      if (trace && warning.stack) {
        console.error(`${prefix}${warning.stack}`);
      } else {
        var toString = warning.toString;
        if (typeof toString !== 'function')
          toString = Error.prototype.toString;
        console.error(`${prefix}${toString.apply(warning)}`);
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
    if (warning.name === 'DeprecationWarning') {
      if (process.noDeprecation)
        return;
      if (process.throwDeprecation)
        throw warning;
    }
    process.nextTick(() => process.emit('warning', warning));
  };
}
