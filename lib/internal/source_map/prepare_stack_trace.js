'use strict';

const {
  Error,
} = primordials;

const debug = require('internal/util/debuglog').debuglog('source_map');
const { findSourceMap } = require('internal/source_map/source_map_cache');
const {
  kNoOverride,
  overrideStackTrace,
  maybeOverridePrepareStackTrace
} = require('internal/errors');

// Create a prettified stacktrace, inserting context from source maps
// if possible.
const ErrorToString = Error.prototype.toString; // Capture original toString.
const prepareStackTrace = (globalThis, error, trace) => {
  // API for node internals to override error stack formatting
  // without interfering with userland code.
  // TODO(bcoe): add support for source-maps to repl.
  if (overrideStackTrace.has(error)) {
    const f = overrideStackTrace.get(error);
    overrideStackTrace.delete(error);
    return f(error, trace);
  }

  const globalOverride =
    maybeOverridePrepareStackTrace(globalThis, error, trace);
  if (globalOverride !== kNoOverride) return globalOverride;

  const errorString = ErrorToString.call(error);

  if (trace.length === 0) {
    return errorString;
  }
  const preparedTrace = trace.map((t, i) => {
    let str = i !== 0 ? '\n    at ' : '';
    str = `${str}${t}`;
    try {
      const sm = findSourceMap(t.getFileName(), error);
      if (sm) {
        // Source Map V3 lines/columns use zero-based offsets whereas, in
        // stack traces, they start at 1/1.
        const {
          originalLine,
          originalColumn,
          originalSource
        } = sm.findEntry(t.getLineNumber() - 1, t.getColumnNumber() - 1);
        if (originalSource && originalLine !== undefined &&
            originalColumn !== undefined) {
          str +=
`\n        -> ${originalSource.replace('file://', '')}:${originalLine + 1}:${originalColumn + 1}`;
        }
      }
    } catch (err) {
      debug(err.stack);
    }
    return str;
  });
  return `${errorString}\n    at ${preparedTrace.join('')}`;
};

module.exports = {
  prepareStackTrace,
};
