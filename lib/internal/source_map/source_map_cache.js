'use strict';

// See https://sourcemaps.info/spec.html for SourceMap V3 specification.
const { Buffer } = require('buffer');
const debug = require('internal/util/debuglog').debuglog('source_map');
const { dirname, resolve } = require('path');
const fs = require('fs');
const { getOptionValue } = require('internal/options');
const {
  normalizeReferrerURL,
} = require('internal/modules/cjs/helpers');
const { JSON, Object } = primordials;
// For cjs, since Module._cache is exposed to users, we use a WeakMap
// keyed on module, facilitating garbage collection.
const cjsSourceMapCache = new WeakMap();
// The esm cache is not exposed to users, so we can use a Map keyed
// on filenames.
const esmSourceMapCache = new Map();
const { fileURLToPath, URL } = require('url');
const { overrideStackTrace } = require('internal/errors');

let experimentalSourceMaps;
function maybeCacheSourceMap(filename, content, cjsModuleInstance) {
  if (experimentalSourceMaps === undefined) {
    experimentalSourceMaps = getOptionValue('--enable-source-maps');
  }
  if (!(process.env.NODE_V8_COVERAGE || experimentalSourceMaps)) return;
  let basePath;
  try {
    filename = normalizeReferrerURL(filename);
    basePath = dirname(fileURLToPath(filename));
  } catch (err) {
    // This is most likely an [eval]-wrapper, which is currently not
    // supported.
    debug(err.stack);
    return;
  }

  const match = content.match(/\/[*/]#\s+sourceMappingURL=(?<sourceMappingURL>[^\s]+)/);
  if (match) {
    if (cjsModuleInstance) {
      cjsSourceMapCache.set(cjsModuleInstance, {
        filename,
        url: match.groups.sourceMappingURL,
        data: dataFromUrl(basePath, match.groups.sourceMappingURL)
      });
    } else {
      // If there is no cjsModuleInstance assume we are in a
      // "modules/esm" context.
      esmSourceMapCache.set(filename, {
        url: match.groups.sourceMappingURL,
        data: dataFromUrl(basePath, match.groups.sourceMappingURL)
      });
    }
  }
}

function dataFromUrl(basePath, sourceMappingURL) {
  try {
    const url = new URL(sourceMappingURL);
    switch (url.protocol) {
      case 'data:':
        return sourceMapFromDataUrl(basePath, url.pathname);
      default:
        debug(`unknown protocol ${url.protocol}`);
        return null;
    }
  } catch (err) {
    debug(err.stack);
    // If no scheme is present, we assume we are dealing with a file path.
    const sourceMapFile = resolve(basePath, sourceMappingURL);
    return sourceMapFromFile(sourceMapFile);
  }
}

function sourceMapFromFile(sourceMapFile) {
  try {
    const content = fs.readFileSync(sourceMapFile, 'utf8');
    const data = JSON.parse(content);
    return sourcesToAbsolute(dirname(sourceMapFile), data);
  } catch (err) {
    debug(err.stack);
    return null;
  }
}

// data:[<mediatype>][;base64],<data> see:
// https://tools.ietf.org/html/rfc2397#section-2
function sourceMapFromDataUrl(basePath, url) {
  const [format, data] = url.split(',');
  const splitFormat = format.split(';');
  const contentType = splitFormat[0];
  const base64 = splitFormat[splitFormat.length - 1] === 'base64';
  if (contentType === 'application/json') {
    const decodedData = base64 ?
      Buffer.from(data, 'base64').toString('utf8') : data;
    try {
      const parsedData = JSON.parse(decodedData);
      return sourcesToAbsolute(basePath, parsedData);
    } catch (err) {
      debug(err.stack);
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
function sourcesToAbsolute(base, data) {
  data.sources = data.sources.map((source) => {
    source = (data.sourceRoot || '') + source;
    if (!/^[\\/]/.test(source[0])) {
      source = resolve(base, source);
    }
    if (!source.startsWith('file://')) source = `file://${source}`;
    return source;
  });
  // The sources array is now resolved to absolute URLs, sourceRoot should
  // be updated to noop.
  data.sourceRoot = '';
  return data;
}

// Move source map from garbage collected module to alternate key.
function rekeySourceMap(cjsModuleInstance, newInstance) {
  const sourceMap = cjsSourceMapCache.get(cjsModuleInstance);
  if (sourceMap) {
    cjsSourceMapCache.set(newInstance, sourceMap);
  }
}

// Get serialized representation of source-map cache, this is used
// to persist a cache of source-maps to disk when NODE_V8_COVERAGE is enabled.
function sourceMapCacheToObject() {
  const obj = Object.create(null);

  for (const [k, v] of esmSourceMapCache) {
    obj[k] = v;
  }
  appendCJSCache(obj);

  if (Object.keys(obj).length === 0) {
    return undefined;
  } else {
    return obj;
  }
}

// Since WeakMap can't be iterated over, we use Module._cache's
// keys to facilitate Source Map serialization.
//
// TODO(bcoe): this means we don't currently serialize source-maps attached
// to error instances, only module instances.
function appendCJSCache(obj) {
  const { Module } = require('internal/modules/cjs/loader');
  Object.keys(Module._cache).forEach((key) => {
    const value = cjsSourceMapCache.get(Module._cache[key]);
    if (value) {
      obj[`file://${key}`] = {
        url: value.url,
        data: value.data
      };
    }
  });
}

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

// Attempt to lookup a source map, which is either attached to a file URI, or
// keyed on an error instance.
function findSourceMap(uri, error) {
  const { Module } = require('internal/modules/cjs/loader');
  let sourceMap = cjsSourceMapCache.get(Module._cache[uri]);
  if (!uri.startsWith('file://')) uri = normalizeReferrerURL(uri);
  if (sourceMap === undefined) {
    sourceMap = esmSourceMapCache.get(uri);
  }
  if (sourceMap === undefined) {
    const candidateSourceMap = cjsSourceMapCache.get(error);
    if (candidateSourceMap && uri === candidateSourceMap.filename) {
      sourceMap = candidateSourceMap;
    }
  }
  return sourceMap;
}

module.exports = {
  maybeCacheSourceMap,
  prepareStackTrace,
  rekeySourceMap,
  sourceMapCacheToObject,
};
