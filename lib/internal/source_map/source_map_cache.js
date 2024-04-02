'use strict';

const {
  ArrayPrototypeMap,
  JSONParse,
  ObjectCreate,
  ObjectKeys,
  ObjectGetOwnPropertyDescriptor,
  ObjectPrototypeHasOwnProperty,
  RegExpPrototypeExec,
  RegExpPrototypeSymbolSplit,
  SafeMap,
  StringPrototypeSplit,
} = primordials;

function ObjectGetValueSafe(obj, key) {
  const desc = ObjectGetOwnPropertyDescriptor(obj, key);
  return ObjectPrototypeHasOwnProperty(desc, 'value') ? desc.value : undefined;
}

// See https://sourcemaps.info/spec.html for SourceMap V3 specification.
const { Buffer } = require('buffer');
let debug = require('internal/util/debuglog').debuglog('source_map', (fn) => {
  debug = fn;
});

const { validateBoolean } = require('internal/validators');
const {
  setSourceMapsEnabled: setSourceMapsNative,
  setPrepareStackTraceCallback,
} = internalBinding('errors');
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
      prepareStackTrace,
    } = require('internal/source_map/prepare_stack_trace');
    setPrepareStackTraceCallback(prepareStackTrace);
  } else if (sourceMapsEnabled !== undefined) {
    // Reset prepare stack trace callback only when disabling source maps.
    const {
      prepareStackTrace,
    } = require('internal/errors');
    setPrepareStackTraceCallback(prepareStackTrace);
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
  try {
    const { normalizeReferrerURL } = require('internal/modules/helpers');
    filename = normalizeReferrerURL(filename);
  } catch (err) {
    // This is most likely an invalid filename in sourceURL of [eval]-wrapper.
    debug(err);
    return;
  }

  if (sourceMapURL === undefined) {
    sourceMapURL = extractSourceMapURLMagicComment(content);
  }

  // Bail out when there is no source map url.
  if (typeof sourceMapURL !== 'string') {
    return;
  }

  if (sourceURL === undefined) {
    sourceURL = extractSourceURLMagicComment(content);
  }

  const data = dataFromUrl(filename, sourceMapURL);
  const url = data ? null : sourceMapURL;
  if (cjsModuleInstance) {
    getCjsSourceMapCache().set(cjsModuleInstance, {
      filename,
      lineLengths: lineLengths(content),
      data,
      url,
      sourceURL,
    });
  } else if (isGeneratedSource) {
    const entry = {
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
  // We purposefully keep \r as part of the line-length calculation, in
  // cases where there is a \r\n separator, so that this can be taken into
  // account in coverage calculations.
  return ArrayPrototypeMap(RegExpPrototypeSymbolSplit(/\n|\u2028|\u2029/, content), (line) => {
    return line.length;
  });
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
function sourcesToAbsolute(baseURL, data) {
  data.sources = data.sources.map((source) => {
    source = (data.sourceRoot || '') + source;
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
  const obj = ObjectCreate(null);

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
    obj[ObjectGetValueSafe(value, 'filename')] = {
      lineLengths: ObjectGetValueSafe(value, 'lineLengths'),
      data: ObjectGetValueSafe(value, 'data'),
      url: ObjectGetValueSafe(value, 'url'),
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
  let sourceMap = esmSourceMapCache.get(sourceURL) ?? generatedSourceMapCache.get(sourceURL);
  if (sourceMap === undefined) {
    for (const value of getCjsSourceMapCache()) {
      const filename = ObjectGetValueSafe(value, 'filename');
      const cachedSourceURL = ObjectGetValueSafe(value, 'sourceURL');
      if (sourceURL === filename || sourceURL === cachedSourceURL) {
        sourceMap = {
          data: ObjectGetValueSafe(value, 'data'),
        };
      }
    }
  }
  if (sourceMap && sourceMap.data) {
    return new SourceMap(sourceMap.data);
  }
  return undefined;
}

module.exports = {
  findSourceMap,
  getSourceMapsEnabled,
  setSourceMapsEnabled,
  maybeCacheSourceMap,
  maybeCacheGeneratedSourceMap,
  sourceMapCacheToObject,
};
