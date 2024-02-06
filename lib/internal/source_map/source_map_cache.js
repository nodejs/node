'use strict';

const {
  ArrayPrototypePush,
  JSONParse,
  ObjectKeys,
  RegExpPrototypeExec,
  SafeMap,
  StringPrototypeCodePointAt,
  StringPrototypeSplit,
} = primordials;

// See https://sourcemaps.info/spec.html for SourceMap V3 specification.
const { Buffer } = require('buffer');
let debug = require('internal/util/debuglog').debuglog('source_map', (fn) => {
  debug = fn;
});

const { validateBoolean } = require('internal/validators');
const {
  setSourceMapsEnabled: setSourceMapsNative,
} = internalBinding('errors');
const {
  setInternalPrepareStackTrace,
} = require('internal/errors');
const { getLazy } = require('internal/util');

// Since the CJS module cache is mutable, which leads to memory leaks when
// modules are deleted, we use a WeakMap so that the source map cache will
// be purged automatically:
const getCjsSourceMapCache = getLazy(() => {
  const { IterableWeakMap } = require('internal/util/iterable_weak_map');
  return new IterableWeakMap();
});

// The esm cache is not mutable, so we can use a Map without memory concerns:
const esmSourceMapCache = new SafeMap();
// The generated sources is not mutable, so we can use a Map without memory concerns:
const generatedSourceMapCache = new SafeMap();
const kLeadingProtocol = /^\w+:\/\//;
const kSourceMappingURLMagicComment = /\/[*/]#\s+sourceMappingURL=(?<sourceMappingURL>[^\s]+)/g;
const kSourceURLMagicComment = /\/[*/]#\s+sourceURL=(?<sourceURL>[^\s]+)/g;

const { isAbsolute } = require('path');
const { fileURLToPath, pathToFileURL, URL } = require('internal/url');

let SourceMap;

// This is configured with --enable-source-maps during pre-execution.
let sourceMapsEnabled = false;
function getSourceMapsEnabled() {
  return sourceMapsEnabled;
}

function setSourceMapsEnabled(val) {
  validateBoolean(val, 'val');

  setSourceMapsNative(val);
  if (val) {
    const {
      prepareStackTraceWithSourceMaps,
    } = require('internal/source_map/prepare_stack_trace');
    setInternalPrepareStackTrace(prepareStackTraceWithSourceMaps);
  } else if (sourceMapsEnabled !== undefined) {
    // Reset prepare stack trace callback only when disabling source maps.
    const {
      defaultPrepareStackTrace,
    } = require('internal/errors');
    setInternalPrepareStackTrace(defaultPrepareStackTrace);
  }

  sourceMapsEnabled = val;
}

function extractSourceURLMagicComment(content) {
  let match;
  let matchSourceURL;
  // A while loop is used here to get the last occurrence of sourceURL.
  // This is needed so that we don't match sourceURL in string literals.
  while ((match = RegExpPrototypeExec(kSourceURLMagicComment, content))) {
    matchSourceURL = match;
  }
  if (matchSourceURL == null) {
    return null;
  }
  let sourceURL = matchSourceURL.groups.sourceURL;
  if (sourceURL != null && RegExpPrototypeExec(kLeadingProtocol, sourceURL) === null) {
    sourceURL = pathToFileURL(sourceURL).href;
  }
  return sourceURL;
}

function extractSourceMapURLMagicComment(content) {
  let match;
  let lastMatch;
  // A while loop is used here to get the last occurrence of sourceMappingURL.
  // This is needed so that we don't match sourceMappingURL in string literals.
  while ((match = RegExpPrototypeExec(kSourceMappingURLMagicComment, content))) {
    lastMatch = match;
  }
  if (lastMatch == null) {
    return null;
  }
  return lastMatch.groups.sourceMappingURL;
}

function maybeCacheSourceMap(filename, content, cjsModuleInstance, isGeneratedSource, sourceURL, sourceMapURL) {
  const sourceMapsEnabled = getSourceMapsEnabled();
  if (!(process.env.NODE_V8_COVERAGE || sourceMapsEnabled)) return;
  const { normalizeReferrerURL } = require('internal/modules/helpers');
  filename = normalizeReferrerURL(filename);
  if (filename === undefined) {
    // This is most likely an invalid filename in sourceURL of [eval]-wrapper.
    return;
  }

  if (sourceMapURL === undefined) {
    sourceMapURL = extractSourceMapURLMagicComment(content);
  }

  // Bail out when there is no source map url.
  if (typeof sourceMapURL !== 'string') {
    return;
  }

  // FIXME: callers should obtain sourceURL from v8 and pass it
  // rather than leaving it undefined and extract by regex.
  if (sourceURL === undefined) {
    sourceURL = extractSourceURLMagicComment(content);
  }

  const data = dataFromUrl(filename, sourceMapURL);
  const url = data ? null : sourceMapURL;
  if (cjsModuleInstance) {
    getCjsSourceMapCache().set(cjsModuleInstance, {
      __proto__: null,
      filename,
      lineLengths: lineLengths(content),
      data,
      url,
      sourceURL,
    });
  } else if (isGeneratedSource) {
    const entry = {
      __proto__: null,
      lineLengths: lineLengths(content),
      data,
      url,
      sourceURL,
    };
    generatedSourceMapCache.set(filename, entry);
    if (sourceURL) {
      generatedSourceMapCache.set(sourceURL, entry);
    }
  } else {
    // If there is no cjsModuleInstance and is not generated source assume we are in a
    // "modules/esm" context.
    const entry = {
      __proto__: null,
      lineLengths: lineLengths(content),
      data,
      url,
      sourceURL,
    };
    esmSourceMapCache.set(filename, entry);
    if (sourceURL) {
      esmSourceMapCache.set(sourceURL, entry);
    }
  }
}

function maybeCacheGeneratedSourceMap(content) {
  const sourceMapsEnabled = getSourceMapsEnabled();
  if (!(process.env.NODE_V8_COVERAGE || sourceMapsEnabled)) return;

  const sourceURL = extractSourceURLMagicComment(content);
  if (sourceURL === null) {
    return;
  }
  try {
    maybeCacheSourceMap(sourceURL, content, null, true, sourceURL);
  } catch (err) {
    // This can happen if the filename is not a valid URL.
    // If we fail to cache the source map, we should not fail the whole process.
    debug(err);
  }
}

function dataFromUrl(sourceURL, sourceMappingURL) {
  try {
    const url = new URL(sourceMappingURL);
    switch (url.protocol) {
      case 'data:':
        return sourceMapFromDataUrl(sourceURL, url.pathname);
      default:
        debug(`unknown protocol ${url.protocol}`);
        return null;
    }
  } catch (err) {
    debug(err);
    // If no scheme is present, we assume we are dealing with a file path.
    const mapURL = new URL(sourceMappingURL, sourceURL).href;
    return sourceMapFromFile(mapURL);
  }
}

// Cache the length of each line in the file that a source map was extracted
// from. This allows translation from byte offset V8 coverage reports,
// to line/column offset Source Map V3.
function lineLengths(content) {
  const contentLength = content.length;
  const output = [];
  let lineLength = 0;
  for (let i = 0; i < contentLength; i++, lineLength++) {
    const codePoint = StringPrototypeCodePointAt(content, i);

    // We purposefully keep \r as part of the line-length calculation, in
    // cases where there is a \r\n separator, so that this can be taken into
    // account in coverage calculations.
    // codepoints for \n (new line), \u2028 (line separator) and \u2029 (paragraph separator)
    if (codePoint === 10 || codePoint === 0x2028 || codePoint === 0x2029) {
      ArrayPrototypePush(output, lineLength);
      lineLength = -1; // To not count the matched codePoint such as \n character
    }
  }
  ArrayPrototypePush(output, lineLength);
  return output;
}


function sourceMapFromFile(mapURL) {
  try {
    const fs = require('fs');
    const content = fs.readFileSync(fileURLToPath(mapURL), 'utf8');
    const data = JSONParse(content);
    return sourcesToAbsolute(mapURL, data);
  } catch (err) {
    debug(err);
    return null;
  }
}

// data:[<mediatype>][;base64],<data> see:
// https://tools.ietf.org/html/rfc2397#section-2
function sourceMapFromDataUrl(sourceURL, url) {
  const { 0: format, 1: data } = StringPrototypeSplit(url, ',');
  const splitFormat = StringPrototypeSplit(format, ';');
  const contentType = splitFormat[0];
  const base64 = splitFormat[splitFormat.length - 1] === 'base64';
  if (contentType === 'application/json') {
    const decodedData = base64 ?
      Buffer.from(data, 'base64').toString('utf8') : data;
    try {
      const parsedData = JSONParse(decodedData);
      return sourcesToAbsolute(sourceURL, parsedData);
    } catch (err) {
      debug(err);
      return null;
    }
  } else {
    debug(`unknown content-type ${contentType}`);
    return null;
  }
}

// If the sources are not absolute URLs after prepending of the "sourceRoot",
// the sources are resolved relative to the SourceMap (like resolving script
// src in a html document).
// If the sources are absolute paths, the sources are converted to absolute file URLs.
function sourcesToAbsolute(baseURL, data) {
  data.sources = data.sources.map((source) => {
    source = (data.sourceRoot || '') + source;
    if (isAbsolute(source)) {
      return pathToFileURL(source).href;
    }
    return new URL(source, baseURL).href;
  });
  // The sources array is now resolved to absolute URLs, sourceRoot should
  // be updated to noop.
  data.sourceRoot = '';
  return data;
}

// WARNING: The `sourceMapCacheToObject` and `appendCJSCache` run during
// shutdown. In particular, they also run when Workers are terminated, making
// it important that they do not call out to any user-provided code, including
// built-in prototypes that might have been tampered with.

// Get serialized representation of source-map cache, this is used
// to persist a cache of source-maps to disk when NODE_V8_COVERAGE is enabled.
function sourceMapCacheToObject() {
  const obj = { __proto__: null };

  for (const { 0: k, 1: v } of esmSourceMapCache) {
    obj[k] = v;
  }

  appendCJSCache(obj);

  if (ObjectKeys(obj).length === 0) {
    return undefined;
  }
  return obj;
}

function appendCJSCache(obj) {
  for (const value of getCjsSourceMapCache()) {
    obj[value.filename] = {
      __proto__: null,
      lineLengths: value.lineLengths,
      data: value.data,
      url: value.url,
    };
  }
}

function findSourceMap(sourceURL) {
  if (RegExpPrototypeExec(kLeadingProtocol, sourceURL) === null) {
    sourceURL = pathToFileURL(sourceURL).href;
  }
  if (!SourceMap) {
    SourceMap = require('internal/source_map/source_map').SourceMap;
  }
  let entry = esmSourceMapCache.get(sourceURL) ?? generatedSourceMapCache.get(sourceURL);
  if (entry === undefined) {
    for (const value of getCjsSourceMapCache()) {
      const filename = value.filename;
      const cachedSourceURL = value.sourceURL;
      if (sourceURL === filename || sourceURL === cachedSourceURL) {
        entry = value;
      }
    }
  }
  if (entry === undefined) {
    return undefined;
  }
  let sourceMap = entry.sourceMap;
  if (sourceMap === undefined) {
    sourceMap = new SourceMap(entry.data, { lineLengths: entry.lineLengths });
    entry.sourceMap = sourceMap;
  }
  return sourceMap;
}

module.exports = {
  findSourceMap,
  getSourceMapsEnabled,
  setSourceMapsEnabled,
  maybeCacheSourceMap,
  maybeCacheGeneratedSourceMap,
  sourceMapCacheToObject,
};
