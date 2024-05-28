'use strict';

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ErrorPrototypeToString,
  RegExpPrototypeSymbolSplit,
  SafeStringIterator,
  StringPrototypeRepeat,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
} = primordials;

let debug = require('internal/util/debuglog').debuglog('source_map', (fn) => {
  debug = fn;
});
const { getStringWidth } = require('internal/util/inspect');
const { readFileSync } = require('fs');
const { findSourceMap } = require('internal/source_map/source_map_cache');
const {
  kIsNodeError,
} = require('internal/errors');
const { fileURLToPath } = require('internal/url');
const { setGetSourceMapErrorSource } = internalBinding('errors');

// Create a prettified stacktrace, inserting context from source maps
// if possible.
function prepareStackTraceWithSourceMaps(error, trace) {
  let errorString;
  if (kIsNodeError in error) {
    errorString = `${error.name} [${error.code}]: ${error.message}`;
  } else {
    errorString = ErrorPrototypeToString(error);
  }

  if (trace.length === 0) {
    return errorString;
  }

  let lastSourceMap;
  let lastFileName;
  const preparedTrace = ArrayPrototypeJoin(ArrayPrototypeMap(trace, (t, i) => {
    const str = '\n    at ';
    try {
      // A stack trace will often have several call sites in a row within the
      // same file, cache the source map and file content accordingly:
      let fileName = t.getFileName();
      if (fileName === undefined) {
        fileName = t.getEvalOrigin();
      }
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
          // Construct call site name based on: v8.dev/docs/stack-trace-api:
          const fnName = t.getFunctionName() ?? t.getMethodName();
          const typeName = t.getTypeName();
          const namePrefix = typeName !== null && typeName !== 'global' ? `${typeName}.` : '';
          const originalName = `${namePrefix}${fnName || '<anonymous>'}`;
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
  return `${errorString}${preparedTrace}`;
}

// Transpilers may have removed the original symbol name used in the stack
// trace, if possible restore it from the names field of the source map:
function getOriginalSymbolName(sourceMap, trace, curIndex) {
  // First check for a symbol name associated with the enclosing function:
  const enclosingEntry = sourceMap.findEntry(
    trace[curIndex].getEnclosingLineNumber() - 1,
    trace[curIndex].getEnclosingColumnNumber() - 1,
  );
  if (enclosingEntry.name) return enclosingEntry.name;
  // Fallback to using the symbol name attached to the next stack frame:
  const currentFileName = trace[curIndex].getFileName();
  const nextCallSite = trace[curIndex + 1];
  if (nextCallSite && currentFileName === nextCallSite.getFileName()) {
    const { name } = sourceMap.findEntry(
      nextCallSite.getLineNumber() - 1,
      nextCallSite.getColumnNumber() - 1,
    );
    return name;
  }
}

/**
 * Return a snippet of code from where the exception was originally thrown
 * above the stack trace. This called from GetErrorSource in node_errors.cc.
 * @param {import('internal/source_map/source_map').SourceMap} sourceMap - the source map to be used
 * @param {string} originalSourcePath - path or url of the original source
 * @param {number} originalLine - line number in the original source
 * @param {number} originalColumn - column number in the original source
 * @returns {string | undefined} - the exact line in the source content or undefined if file not found
 */
function getErrorSource(
  sourceMap,
  originalSourcePath,
  originalLine,
  originalColumn,
) {
  const originalSourcePathNoScheme =
    StringPrototypeStartsWith(originalSourcePath, 'file://') ?
      fileURLToPath(originalSourcePath) : originalSourcePath;
  const source = getOriginalSource(
    sourceMap.payload,
    originalSourcePath,
  );
  if (typeof source !== 'string') {
    return;
  }
  const lines = RegExpPrototypeSymbolSplit(/\r?\n/, source, originalLine + 1);
  const line = lines[originalLine];
  if (!line) {
    return;
  }

  // Display ^ in appropriate position, regardless of whether tabs or
  // spaces are used:
  let prefix = '';
  for (const character of new SafeStringIterator(
    StringPrototypeSlice(line, 0, originalColumn + 1))) {
    prefix += character === '\t' ? '\t' :
      StringPrototypeRepeat(' ', getStringWidth(character));
  }
  prefix = StringPrototypeSlice(prefix, 0, -1); // The last character is '^'.

  const exceptionLine =
   `${originalSourcePathNoScheme}:${originalLine + 1}\n${line}\n${prefix}^\n\n`;
  return exceptionLine;
}

/**
 * Retrieve the original source code from the source map's `sources` list or disk.
 * @param {import('internal/source_map/source_map').SourceMap.payload} payload
 * @param {string} originalSourcePath - path or url of the original source
 * @returns {string | undefined} - the source content or undefined if file not found
 */
function getOriginalSource(payload, originalSourcePath) {
  let source;
  // payload.sources has been normalized to be an array of absolute urls.
  const sourceContentIndex =
    ArrayPrototypeIndexOf(payload.sources, originalSourcePath);
  if (payload.sourcesContent?.[sourceContentIndex]) {
    // First we check if the original source content was provided in the
    // source map itself:
    source = payload.sourcesContent[sourceContentIndex];
  } else if (StringPrototypeStartsWith(originalSourcePath, 'file://')) {
    // If no sourcesContent was found, attempt to load the original source
    // from disk:
    debug(`read source of ${originalSourcePath} from filesystem`);
    const originalSourcePathNoScheme = fileURLToPath(originalSourcePath);
    try {
      source = readFileSync(originalSourcePathNoScheme, 'utf8');
    } catch (err) {
      debug(err);
    }
  }
  return source;
}

/**
 * Retrieve exact line in the original source code from the source map's `sources` list or disk.
 * @param {string} fileName - actual file name
 * @param {number} lineNumber - actual line number
 * @param {number} columnNumber - actual column number
 * @returns {string | undefined} - the source content or undefined if file not found
 */
function getSourceMapErrorSource(fileName, lineNumber, columnNumber) {
  const sm = findSourceMap(fileName);
  if (sm === undefined) {
    return;
  }
  const {
    originalLine,
    originalColumn,
    originalSource,
  } = sm.findEntry(lineNumber - 1, columnNumber);
  const errorSource = getErrorSource(sm, originalSource, originalLine, originalColumn);
  return errorSource;
}

setGetSourceMapErrorSource(getSourceMapErrorSource);

module.exports = {
  prepareStackTraceWithSourceMaps,
};
