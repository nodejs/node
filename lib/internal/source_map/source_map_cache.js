'use strict';

const {
  JSONParse,
  ObjectCreate,
  ObjectKeys,
  ObjectGetOwnPropertyDescriptor,
  ObjectPrototypeHasOwnProperty,
  Map,
  MapPrototypeEntries,
  WeakMap,
  WeakMapPrototypeGet,
  uncurryThis,
} = primordials;

const MapIteratorNext = uncurryThis(MapPrototypeEntries(new Map()).next);

function ObjectGetValueSafe(obj, key) {
  const desc = ObjectGetOwnPropertyDescriptor(obj, key);
  return ObjectPrototypeHasOwnProperty(desc, 'value') ? desc.value : undefined;
}

// See https://sourcemaps.info/spec.html for SourceMap V3 specification.
const { Buffer } = require('buffer');
const debug = require('internal/util/debuglog').debuglog('source_map');
const { dirname, resolve } = require('path');
const fs = require('fs');
const { getOptionValue } = require('internal/options');
const {
  normalizeReferrerURL,
} = require('internal/modules/cjs/helpers');
// For cjs, since Module._cache is exposed to users, we use a WeakMap
// keyed on module, facilitating garbage collection.
const cjsSourceMapCache = new WeakMap();
// The esm cache is not exposed to users, so we can use a Map keyed
// on filenames.
const esmSourceMapCache = new Map();
const { fileURLToPath, URL } = require('url');
let Module;
let SourceMap;

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
    const data = dataFromUrl(basePath, match.groups.sourceMappingURL);
    const url = data ? null : match.groups.sourceMappingURL;
    if (cjsModuleInstance) {
      if (!Module) Module = require('internal/modules/cjs/loader').Module;
      cjsSourceMapCache.set(cjsModuleInstance, {
        filename,
        lineLengths: lineLengths(content),
        data,
        url
      });
    } else {
      // If there is no cjsModuleInstance assume we are in a
      // "modules/esm" context.
      esmSourceMapCache.set(filename, {
        lineLengths: lineLengths(content),
        data,
        url
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

// Cache the length of each line in the file that a source map was extracted
// from. This allows translation from byte offset V8 coverage reports,
// to line/column offset Source Map V3.
function lineLengths(content) {
  // We purposefully keep \r as part of the line-length calculation, in
  // cases where there is a \r\n separator, so that this can be taken into
  // account in coverage calculations.
  return content.split(/\n|\u2028|\u2029/).map((line) => {
    return line.length;
  });
}

function sourceMapFromFile(sourceMapFile) {
  try {
    const content = fs.readFileSync(sourceMapFile, 'utf8');
    const data = JSONParse(content);
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
      const parsedData = JSONParse(decodedData);
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

// WARNING: The `sourceMapCacheToObject` and `appendCJSCache` run during
// shutdown. In particular, they also run when Workers are terminated, making
// it important that they do not call out to any user-provided code, including
// built-in prototypes that might have been tampered with.

// Get serialized representation of source-map cache, this is used
// to persist a cache of source-maps to disk when NODE_V8_COVERAGE is enabled.
function sourceMapCacheToObject() {
  const obj = ObjectCreate(null);

  const it = MapPrototypeEntries(esmSourceMapCache);
  let entry;
  while (!(entry = MapIteratorNext(it)).done) {
    const k = entry.value[0];
    const v = entry.value[1];
    obj[k] = v;
  }

  appendCJSCache(obj);

  if (ObjectKeys(obj).length === 0) {
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
  if (!Module) return;
  const cjsModuleCache = ObjectGetValueSafe(Module, '_cache');
  const cjsModules = ObjectKeys(cjsModuleCache);
  for (let i = 0; i < cjsModules.length; i++) {
    const key = cjsModules[i];
    const module = ObjectGetValueSafe(cjsModuleCache, key);
    const value = WeakMapPrototypeGet(cjsSourceMapCache, module);
    if (value) {
      // This is okay because `obj` has a null prototype.
      obj[`file://${key}`] = {
        lineLengths: ObjectGetValueSafe(value, 'lineLengths'),
        data: ObjectGetValueSafe(value, 'data'),
        url: ObjectGetValueSafe(value, 'url')
      };
    }
  }
}

// Attempt to lookup a source map, which is either attached to a file URI, or
// keyed on an error instance.
// TODO(bcoe): once WeakRefs are available in Node.js, refactor to drop
// requirement of error parameter.
function findSourceMap(uri, error) {
  if (!Module) Module = require('internal/modules/cjs/loader').Module;
  if (!SourceMap) {
    SourceMap = require('internal/source_map/source_map').SourceMap;
  }
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
  if (sourceMap && sourceMap.data) {
    return new SourceMap(sourceMap.data);
  } else {
    return undefined;
  }
}

module.exports = {
  findSourceMap,
  maybeCacheSourceMap,
  rekeySourceMap,
  sourceMapCacheToObject,
};
