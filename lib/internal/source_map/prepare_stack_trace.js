'use strict';

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ErrorPrototypeToString,
  StringPrototypeRepeat,
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
} = primordials;

let debug = require('internal/util/debuglog').debuglog('source_map', (fn) => {
  debug = fn;
});
const { getStringWidth } = require('internal/util/inspect');
const { readFileSync } = require('fs');
const { findSourceMap } = require('internal/source_map/source_map_cache');
const {
  kNoOverride,
  overrideStackTrace,
  maybeOverridePrepareStackTrace
} = require('internal/errors');
const { fileURLToPath } = require('internal/url');

// Create a prettified stacktrace, inserting context from source maps
// if possible.
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

  const errorString = ErrorPrototypeToString(error);

  if (trace.length === 0) {
    return errorString;
  }

  let errorSource = '';
  let firstLine;
  let firstColumn;
  const preparedTrace = ArrayPrototypeJoin(ArrayPrototypeMap(trace, (t, i) => {
    if (i === 0) {
      firstLine = t.getLineNumber();
      firstColumn = t.getColumnNumber();
    }
    let str = i !== 0 ? '\n    at ' : '';
    str = `${str}${t}`;
    try {
      const sm = findSourceMap(t.getFileName());
      if (sm) {
        // Source Map V3 lines/columns use zero-based offsets whereas, in
        // stack traces, they start at 1/1.
        const {
          originalLine,
          originalColumn,
          originalSource,
          name
        } = sm.findEntry(t.getLineNumber() - 1, t.getColumnNumber() - 1);
        if (originalSource && originalLine !== undefined &&
            originalColumn !== undefined) {
          if (i === 0) {
            firstLine = originalLine + 1;
            firstColumn = originalColumn + 1;
            // Show error in original source context to help user pinpoint it:
            errorSource = getErrorSource(
              sm.payload,
              originalSource,
              firstLine,
              firstColumn
            );
          }
          // Show both original and transpiled stack trace information:
          const prefix = name ? `\n        -> at ${name}` : '\n        ->';
          const originalSourceNoScheme =
            StringPrototypeStartsWith(originalSource, 'file://') ?
              fileURLToPath(originalSource) : originalSource;
          str += `${prefix} (${originalSourceNoScheme}:${originalLine + 1}:` +
            `${originalColumn + 1})`;
        }
      }
    } catch (err) {
      debug(err.stack);
    }
    return str;
  }), '');
  return `${errorSource}${errorString}\n    at ${preparedTrace}`;
};

// Places a snippet of code from where the exception was originally thrown
// above the stack trace. This logic is modeled after GetErrorSource in
// node_errors.cc.
function getErrorSource(payload, originalSource, firstLine, firstColumn) {
  let exceptionLine = '';
  const originalSourceNoScheme =
    StringPrototypeStartsWith(originalSource, 'file://') ?
      fileURLToPath(originalSource) : originalSource;

  let source;
  const sourceContentIndex =
    ArrayPrototypeIndexOf(payload.sources, originalSource);
  if (payload.sourcesContent?.[sourceContentIndex]) {
    // First we check if the original source content was provided in the
    // source map itself:
    source = payload.sourcesContent[sourceContentIndex];
  } else {
    // If no sourcesContent was found, attempt to load the original source
    // from disk:
    try {
      source = readFileSync(originalSourceNoScheme, 'utf8');
    } catch (err) {
      debug(err);
      return '';
    }
  }

  const lines = StringPrototypeSplit(source, /\r?\n/, firstLine);
  const line = lines[firstLine - 1];
  if (!line) return exceptionLine;

  // Display ^ in appropriate position, regardless of whether tabs or
  // spaces are used:
  let prefix = '';
  for (const character of StringPrototypeSlice(line, 0, firstColumn)) {
    prefix += (character === '\t') ? '\t' :
      StringPrototypeRepeat(' ', getStringWidth(character));
  }
  prefix = StringPrototypeSlice(prefix, 0, -1); // The last character is '^'.

  exceptionLine =
              `${originalSourceNoScheme}:${firstLine}\n${line}\n${prefix}^\n\n`;
  return exceptionLine;
}

module.exports = {
  prepareStackTrace,
};
