'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  JSONParse,
  ObjectFreeze,
  RegExpPrototypeExec,
  RegExpPrototypeSymbolSplit,
  SafeMap,
  StringPrototypeCodePointAt,
  StringPrototypeIndexOf,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
} = primordials;

// See https://tc39.es/ecma426/ for SourceMap V3 specification.
const { Buffer } = require('buffer');
let debug = require('internal/util/debuglog').debuglog('source_map', (fn) => {
  debug = fn;
});

const { readFileSync } = require('fs');
const { validateBoolean, validateObject } = require('internal/validators');
const {
  setSourceMapsEnabled: setSourceMapsNative,
} = internalBinding('errors');
const {
  defaultPrepareStackTrace,
  setInternalPrepareStackTrace,
} = require('internal/errors');
const { getLazy, isUnderNodeModules, kEmptyObject } = require('internal/util');

const getModuleSourceMapCache = getLazy(() => {
  const { SourceMapCacheMap } = require('internal/source_map/source_map_cache_map');
  return new SourceMapCacheMap();
});

// The generated source module/script instance is not accessible, so these entries
// cannot be keyed weakly like the module source entries are. The cache holds the
// least recently used ones within a byte budget instead, or generated sources
// evaluated under a changing source url would retain every payload they ever
// mapped. The budget counts bytes rather than entries because one source map can
// carry hundreds of kilobytes of sourcesContent while the next carries a handful.
// Separate generated source entries with the module source entries to avoid
// overriding the module source entries with arbitrary source url magic comments.
const kGeneratedSourceMapCacheSizeLimit = 32 * 1024 * 1024;
const generatedSourceMapCache = new SafeMap();
let generatedSourceMapCacheSize = 0;
const kLeadingProtocol = /^\w+:\/\//;
const kSourceMappingURLMagicComment = /\/[*/]#\s+sourceMappingURL=(?<sourceMappingURL>[^\s]+)/g;
const kSourceURLMagicComment = /\/[*/]#\s+sourceURL=(?<sourceURL>[^\s]+)/g;

const { isAbsolute } = require('path');
const { fileURLToPath, pathToFileURL, URL, URLParse } = require('internal/url');

let SourceMap;

// This is configured with --enable-source-maps during pre-execution.
let sourceMapsSupport = ObjectFreeze({
  __proto__: null,
  enabled: false,
  nodeModules: false,
  generatedCode: false,
});
function getSourceMapsSupport() {
  // Return a read-only object.
  return sourceMapsSupport;
}

/**
 * Enables or disables source maps programmatically.
 * @param {boolean} enabled
 * @param {object} options
 * @param {boolean} [options.nodeModules]
 * @param {boolean} [options.generatedCode]
 */
function setSourceMapsSupport(enabled, options = kEmptyObject) {
  validateBoolean(enabled, 'enabled');
  validateObject(options, 'options');

  const { nodeModules = false, generatedCode = false } = options;
  validateBoolean(nodeModules, 'options.nodeModules');
  validateBoolean(generatedCode, 'options.generatedCode');

  setSourceMapsNative(enabled);
  if (enabled) {
    const {
      prepareStackTraceWithSourceMaps,
    } = require('internal/source_map/prepare_stack_trace');
    setInternalPrepareStackTrace(prepareStackTraceWithSourceMaps);
  } else {
    setInternalPrepareStackTrace(defaultPrepareStackTrace);
  }

  sourceMapsSupport = ObjectFreeze({
    __proto__: null,
    enabled,
    nodeModules: nodeModules,
    generatedCode: generatedCode,
  });
}

/**
 * Extracts the source url from the content if present. For example
 * //# sourceURL=file:///path/to/file
 *
 * Read more at: https://tc39.es/source-map-spec/#linking-evald-code-to-named-generated-code
 * @param {string} content - source content
 * @returns {string | null} source url or null if not present
 */
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

/**
 * Extracts the source map url from the content if present. For example
 * //# sourceMappingURL=file:///path/to/file
 *
 * Read more at: https://tc39.es/source-map-spec/#linking-generated-code
 * @param {string} content - source content
 * @returns {string | null} source map url or null if not present
 */
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

/**
 * Caches the source map, with the given filename, moduleInstance, sourceURL and sourceMapURL.
 * This function does not automatically extract the source map from the content. The caller should either
 * extract the source map from the content via V8 API or use {@link extractSourceURLMagicComment} explicitly.
 * @param {string} filename - the actual filename
 * @param {string} content - the actual source content
 * @param {import('internal/modules/cjs/loader').Module | ModuleWrap} moduleInstance - a module instance that
 *   associated with the source, once this is reclaimed, the source map entry will be removed from the cache
 * @param {boolean} isGeneratedSource - if the source was generated and evaluated with the global eval
 * @param {string | undefined} sourceURL - the source url
 * @param {string | undefined} sourceMapURL - the source map url
 */
function maybeCacheSourceMap(filename, content, moduleInstance, isGeneratedSource, sourceURL, sourceMapURL) {
  const support = getSourceMapsSupport();
  if (!(process.env.NODE_V8_COVERAGE || support.enabled)) return;
  const { normalizeReferrerURL } = require('internal/modules/helpers');
  filename = normalizeReferrerURL(filename);
  if (filename === undefined) {
    // This is most likely an invalid filename in sourceURL of [eval]-wrapper.
    return;
  }
  if (!support.nodeModules && isUnderNodeModules(filename)) {
    // Skip file under node_modules if not enabled.
    return;
  }

  // Bail out when there is no source map url.
  if (typeof sourceMapURL !== 'string') {
    return;
  }

  // Normalize the sourceURL to a file URL if it is a path.
  sourceURL = normalizeReferrerURL(sourceURL);

  // The payload is resolved on first use (see sourceMapData()), except under
  // coverage, where it is serialized at exit when no more JS may run.
  const entry = {
    __proto__: null,
    lineLengths: lineLengths(content),
    data: process.env.NODE_V8_COVERAGE ? dataFromUrl(filename, sourceMapURL) : undefined,
    filename,
    sourceMapURL,
    sourceURL,
  };

  if (isGeneratedSource) {
    // Only generated entries are charged against the budget, so the presence of
    // a size doubles as the marker for an entry that is accounted for.
    entry.size = sourceMapURL.length + entry.lineLengths.length;
    if (entry.data != null) {
      entry.size += sourceMapPayloadSize(entry.data);
    }
    deleteGeneratedSourceMap(filename);
    generatedSourceMapCache.set(filename, entry);
    generatedSourceMapCacheSize += entry.size;
    // Keep the newest entry even when it alone exceeds the budget, otherwise a
    // single large source map could never be mapped at all.
    while (generatedSourceMapCacheSize > kGeneratedSourceMapCacheSizeLimit &&
           generatedSourceMapCache.size > 1) {
      deleteGeneratedSourceMap(generatedSourceMapCache.keys().next().value);
    }
    return;
  }
  // If it is not a generated source, we assume we are in a "cjs/esm"
  // context.
  const keys = sourceURL ? [filename, sourceURL] : [filename];
  getModuleSourceMapCache().set(keys, entry, moduleInstance);
}

/**
 * Approximate the bytes a resolved payload keeps alive.
 * @param {object} data - deserialized source map JSON object
 * @returns {number} size in bytes
 */
function sourceMapPayloadSize(data) {
  let size = data.mappings?.length ?? 0;
  const sourcesContent = data.sourcesContent;
  if (ArrayIsArray(sourcesContent)) {
    for (let i = 0; i < sourcesContent.length; i++) {
      size += sourcesContent[i]?.length ?? 0;
    }
  }
  return size;
}

/**
 * Drop a generated source entry and give its bytes back to the budget.
 * @param {string} filename - key of the entry
 */
function deleteGeneratedSourceMap(filename) {
  const entry = generatedSourceMapCache.get(filename);
  if (entry === undefined) {
    return;
  }
  generatedSourceMapCacheSize -= entry.size;
  generatedSourceMapCache.delete(filename);
}

/**
 * Caches the source map if it is present in the eval'd source.
 * @param {string} content - the eval'd source code
 */
function maybeCacheGeneratedSourceMap(content) {
  const support = getSourceMapsSupport();
  if (!(process.env.NODE_V8_COVERAGE || support.enabled || support.generated)) return;

  const sourceURL = extractSourceURLMagicComment(content);
  if (sourceURL === null) {
    return;
  }
  const sourceMapURL = extractSourceMapURLMagicComment(content);
  if (sourceMapURL === null) {
    return;
  }

  try {
    // Use the sourceURL as the filename, and do not create a duplicate entry.
    maybeCacheSourceMap(sourceURL, content, null, true, undefined /** no duplicated sourceURL */, sourceMapURL);
  } catch (err) {
    // This can happen if the filename is not a valid URL.
    // If we fail to cache the source map, we should not fail the whole process.
    debug(err);
  }
}

/**
 * Resolves source map payload data from the source url and source map url.
 * If the source map url is a data url, the data is returned.
 * Otherwise the source map url is resolved to a file path and the file is read.
 * @param {string} sourceURL - url of the source file
 * @param {string} sourceMappingURL - url of the source map
 * @returns {object} deserialized source map JSON object
 */
function dataFromUrl(sourceURL, sourceMappingURL) {
  const url = URLParse(sourceMappingURL);

  if (url != null) {
    switch (url.protocol) {
      case 'data:':
        return sourceMapFromDataUrl(sourceURL, url.pathname);
      default:
        debug(`unknown protocol ${url.protocol}`);
        return null;
    }
  }

  const mapURL = URLParse(sourceMappingURL, sourceURL);
  if (mapURL === null) {
    return null;
  }
  return sourceMapFromFile(mapURL);
}

const kUnicodeLineTerminators = /[\u2028\u2029]/;

// Cache the length of each line in the file that a source map was extracted
// from. This allows translation from byte offset V8 coverage reports,
// to line/column offset Source Map V3.
function lineLengths(content) {
  if (RegExpPrototypeExec(kUnicodeLineTerminators, content) !== null) {
    return lineLengthsWithUnicodeTerminators(content);
  }
  // We purposefully keep \r as part of the line-length calculation, in
  // cases where there is a \r\n separator, so that this can be taken into
  // account in coverage calculations.
  const output = [];
  let lineStart = 0;
  let lineEnd;
  while ((lineEnd = StringPrototypeIndexOf(content, '\n', lineStart)) !== -1) {
    ArrayPrototypePush(output, lineEnd - lineStart);
    lineStart = lineEnd + 1;
  }
  ArrayPrototypePush(output, content.length - lineStart);
  return output;
}

function lineLengthsWithUnicodeTerminators(content) {
  const contentLength = content.length;
  const output = [];
  let lineLength = 0;
  for (let i = 0; i < contentLength; i++, lineLength++) {
    const codePoint = StringPrototypeCodePointAt(content, i);
    // \n (new line), \u2028 (line separator) and \u2029 (paragraph separator)
    if (codePoint === 10 || codePoint === 0x2028 || codePoint === 0x2029) {
      ArrayPrototypePush(output, lineLength);
      lineLength = -1; // To not count the matched codePoint such as \n character
    }
  }
  ArrayPrototypePush(output, lineLength);
  return output;
}

/**
 * Read source map from file.
 * @param {URL} mapURL - file url of the source map
 * @returns {object} deserialized source map JSON object
 */
function sourceMapFromFile(mapURL) {
  try {
    const content = readFileSync(fileURLToPath(mapURL), 'utf8');
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
  const { 0: format, 1: data } = StringPrototypeSplit(url, ',', 2);
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
      // TODO(legendecas): warn about invalid source map JSON string.
      // But it could be verbose.
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

// WARNING: The `sourceMapCacheToObject` runs during shutdown. In particular,
// it also runs when Workers are terminated, making it important that it does
// not call out to any user-provided code, including built-in prototypes that
// might have been tampered with.

// Get serialized representation of source-map cache, this is used
// to persist a cache of source-maps to disk when NODE_V8_COVERAGE is enabled.
function sourceMapCacheToObject() {
  const moduleSourceMapCache = getModuleSourceMapCache();
  if (moduleSourceMapCache.size === 0) {
    return undefined;
  }

  const obj = { __proto__: null };
  for (const { 0: k, 1: v } of moduleSourceMapCache) {
    const data = v.data ?? null;
    obj[k] = {
      __proto__: null,
      lineLengths: v.lineLengths,
      data,
      // Save the source map url if it is not a data url.
      url: data ? null : v.sourceMapURL,
    };
  }
  return obj;
}

/**
 * Resolve and parse the payload of a cache entry the first time it is needed;
 * `null` marks a source map that could not be loaded.
 * @param {object} entry
 * @returns {object|null}
 */
function sourceMapData(entry) {
  if (entry.data === undefined) {
    entry.data = dataFromUrl(entry.filename, entry.sourceMapURL);
    if (entry.size !== undefined && entry.data !== null) {
      const size = sourceMapPayloadSize(entry.data);
      entry.size += size;
      generatedSourceMapCacheSize += size;
    }
  }
  return entry.data;
}

/**
 * Find a source map for a given actual source URL or path.
 *
 * This function may be invoked from user code or test runner, this must not throw
 * any exceptions.
 * @param {string} sourceURL - actual source URL or path
 * @returns {import('internal/source_map/source_map').SourceMap | undefined} a source map or undefined if not found
 */
function findSourceMap(sourceURL) {
  if (typeof sourceURL !== 'string') {
    return undefined;
  }

  // No source maps for builtin modules.
  if (sourceURL.startsWith('node:')) {
    return undefined;
  }

  if (!getSourceMapsSupport().nodeModules && isUnderNodeModules(sourceURL)) {
    return undefined;
  }

  SourceMap ??= require('internal/source_map/source_map').SourceMap;
  try {
    if (RegExpPrototypeExec(kLeadingProtocol, sourceURL) === null) {
      // If the sourceURL is an invalid path, this will throw an error.
      sourceURL = pathToFileURL(sourceURL).href;
    }
    let entry = getModuleSourceMapCache().get(sourceURL);
    if (entry === undefined) {
      entry = generatedSourceMapCache.get(sourceURL);
      if (entry !== undefined) {
        // Move the entry back to the newest end, so that a generated source that
        // is still in use is not evicted for being old.
        generatedSourceMapCache.delete(sourceURL);
        generatedSourceMapCache.set(sourceURL, entry);
      }
    }
    if (entry === undefined || sourceMapData(entry) === null) {
      return undefined;
    }

    let sourceMap = entry.sourceMap;
    if (sourceMap === undefined) {
      sourceMap = new SourceMap(entry.data, { lineLengths: entry.lineLengths });
      entry.sourceMap = sourceMap;
    }
    return sourceMap;
  } catch (err) {
    debug(err);
    return undefined;
  }
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
 * Get the line of source in the source map.
 * @param {import('internal/source_map/source_map').SourceMap} sourceMap
 * @param {string} originalSourcePath path or url of the original source
 * @param {number} originalLine line number in the original source
 * @returns {string|undefined} source line if found
 */
function getSourceLine(
  sourceMap,
  originalSourcePath,
  originalLine,
) {
  const source = getOriginalSource(
    sourceMap.payload,
    originalSourcePath,
  );
  if (typeof source !== 'string') {
    return;
  }
  const lines = RegExpPrototypeSymbolSplit(/\r?\n/, source, originalLine + 1);
  const line = lines[originalLine];
  return line;
}

module.exports = {
  kGeneratedSourceMapCacheSizeLimit,
  findSourceMap,
  getSourceLine,
  getSourceMapsSupport,
  setSourceMapsSupport,
  maybeCacheSourceMap,
  maybeCacheGeneratedSourceMap,
  sourceMapCacheToObject,
};
