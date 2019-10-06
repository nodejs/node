'use strict';

const debug = require('internal/util/debuglog').debuglog('source_map');
const { findSourceMap } = require('internal/source_map/source_map_cache');
const { overrideStackTrace } = require('internal/errors');

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

  const { SourceMap } = require('internal/source_map/source_map');
  const errorString = ErrorToString.call(error);

  if (trace.length === 0) {
    return errorString;
  }
  const preparedTrace = trace.map((t, i) => {
    let str = i !== 0 ? '\n    at ' : '';
    str = `${str}${t}`;
    try {
      const sourceMap = findSourceMap(t.getFileName(), error);
      if (sourceMap && sourceMap.data) {
        const sm = new SourceMap(sourceMap.data);
        // Source Map V3 lines/columns use zero-based offsets whereas, in
        // stack traces, they start at 1/1.
        const [, , url, line, col] =
                   sm.findEntry(t.getLineNumber() - 1, t.getColumnNumber() - 1);
        if (url && line !== undefined && col !== undefined) {
          str +=
            `\n        -> ${url.replace('file://', '')}:${line + 1}:${col + 1}`;
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
