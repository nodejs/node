'use strict';

const traceWarnings = process.traceProcessWarnings;
const noDeprecation = process.noDeprecation;
const traceDeprecation = process.traceDeprecation;
const throwDeprecation = process.throwDeprecation;
const prefix = `(${process.release.name}:${process.pid}) `;

exports.setup = setupProcessWarnings;

function setupProcessWarnings() {
  if (!process.noProcessWarnings) {
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
    if (throwDeprecation && warning.name === 'DeprecationWarning')
      throw warning;
    else
      process.nextTick(() => process.emit('warning', warning));
  };
}
