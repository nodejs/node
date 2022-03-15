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
  SafeStringIterator,
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
  maybeOverridePrepareStackTrace,
  kIsNodeError,
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

  let errorString;
  if (kIsNodeError in error) {
    errorString = `${error.name} [${error.code}]: ${error.message}`;
  } else {
    errorString = ErrorPrototypeToString(error);
  }

  if (trace.length === 0) {
    return errorString;
  }

  let errorSource = '';
  let lastSourceMap;
  let lastFileName;
  const preparedTrace = ArrayPrototypeJoin(ArrayPrototypeMap(trace, (t, i) => {
    const str = i !== 0 ? '\n    at ' : '';
    try {
      // A stack trace will often have several call sites in a row within the
      // same file, cache the source map and file content accordingly:
      const fileName = t.getFileName();
      const sm = fileName === lastFileName ?
        lastSourceMap :
        findSourceMap(fileName);
      lastSourceMap = sm;
      lastFileName = fileName;
      if (sm) {
        // Source Map V3 lines/columns start at 0/0 whereas stack traces
        // start at 1/1:
        const {
          originalLine,
          originalColumn,
          originalSource,
        } = sm.findEntry(t.getLineNumber() - 1, t.getColumnNumber() - 1);
        if (originalSource && originalLine !== undefined &&
            originalColumn !== undefined) {
          const name = getOriginalSymbolName(sm, trace, i);
          if (i === 0) {
            errorSource = getErrorSource(
              sm,
              originalSource,
              originalLine,
              originalColumn
            );
          }
          // Construct call site name based on: v8.dev/docs/stack-trace-api:
          const fnName = t.getFunctionName() ?? t.getMethodName();
          const originalName = `${t.getTypeName() !== 'global' ?
            `${t.getTypeName()}.` : ''}${fnName || '<anonymous>'}`;
          // The original call site may have a different symbol name
          // associated with it, use it:
          const prefix = (name && name !== originalName) ?
            `${name}` :
            `${originalName}`;
          const hasName = !!(name || originalName);
          const originalSourceNoScheme =
            StringPrototypeStartsWith(originalSource, 'file://') ?
              fileURLToPath(originalSource) : originalSource;
          // Replace the transpiled call site with the original:
          return `${str}${prefix}${hasName ? ' (' : ''}` +
            `${originalSourceNoScheme}:${originalLine + 1}:` +
            `${originalColumn + 1}${hasName ? ')' : ''}`;
        }
      }
    } catch (err) {
      debug(err);
    }
    return `${str}${t}`;
  }), '');
  return `${errorSource}${errorString}\n    at ${preparedTrace}`;
};

// Transpilers may have removed the original symbol name used in the stack
// trace, if possible restore it from the names field of the source map:
function getOriginalSymbolName(sourceMap, trace, curIndex) {
  // First check for a symbol name associated with the enclosing function:
  const enclosingEntry = sourceMap.findEntry(
    trace[curIndex].getEnclosingLineNumber() - 1,
    trace[curIndex].getEnclosingColumnNumber() - 1
  );
  if (enclosingEntry.name) return enclosingEntry.name;
  // Fallback to using the symbol name attached to the next stack frame:
  const currentFileName = trace[curIndex].getFileName();
  const nextCallSite = trace[curIndex + 1];
  if (nextCallSite && currentFileName === nextCallSite.getFileName()) {
    const { name } = sourceMap.findEntry(
      nextCallSite.getLineNumber() - 1,
      nextCallSite.getColumnNumber() - 1
    );
    return name;
  }
}

// Places a snippet of code from where the exception was originally thrown
// above the stack trace. This logic is modeled after GetErrorSource in
// node_errors.cc.
function getErrorSource(
  sourceMap,
  originalSourcePath,
  originalLine,
  originalColumn
) {
  let exceptionLine = '';
  const originalSourcePathNoScheme =
    StringPrototypeStartsWith(originalSourcePath, 'file://') ?
      fileURLToPath(originalSourcePath) : originalSourcePath;
  const source = getOriginalSource(
    sourceMap.payload,
    originalSourcePathNoScheme
  );
  const lines = StringPrototypeSplit(source, /\r?\n/, originalLine + 1);
  const line = lines[originalLine];
  if (!line) return exceptionLine;

  // Display ^ in appropriate position, regardless of whether tabs or
  // spaces are used:
  let prefix = '';
  for (const character of new SafeStringIterator(
    StringPrototypeSlice(line, 0, originalColumn + 1))) {
    prefix += character === '\t' ? '\t' :
      StringPrototypeRepeat(' ', getStringWidth(character));
  }
  prefix = StringPrototypeSlice(prefix, 0, -1); // The last character is '^'.

  exceptionLine =
   `${originalSourcePathNoScheme}:${originalLine + 1}\n${line}\n${prefix}^\n\n`;
  return exceptionLine;
}

function getOriginalSource(payload, originalSourcePath) {
  let source;
  const originalSourcePathNoScheme =
    StringPrototypeStartsWith(originalSourcePath, 'file://') ?
      fileURLToPath(originalSourcePath) : originalSourcePath;
  const sourceContentIndex =
    ArrayPrototypeIndexOf(payload.sources, originalSourcePath);
  if (payload.sourcesContent?.[sourceContentIndex]) {
    // First we check if the original source content was provided in the
    // source map itself:
    source = payload.sourcesContent[sourceContentIndex];
  } else {
    // If no sourcesContent was found, attempt to load the original source
    // from disk:
    try {
      source = readFileSync(originalSourcePathNoScheme, 'utf8');
    } catch (err) {
      debug(err);
      source = '';
    }
  }
  return source;
}

module.exports = {
  prepareStackTrace,
};
