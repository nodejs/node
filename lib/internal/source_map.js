'use strict';

// See https://sourcemaps.info/spec.html for SourceMap V3 specification.
const { Buffer } = require('buffer');
const debug = require('internal/util/debuglog').debuglog('source_map');
const { dirname, resolve } = require('path');
const fs = require('fs');
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

function maybeCacheSourceMap(filename, content, cjsModuleInstance) {
  if (!process.env.NODE_V8_COVERAGE) return;

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
function appendCJSCache(obj) {
  const { Module } = require('internal/modules/cjs/loader');
  Object.keys(Module._cache).forEach((key) => {
    const value = cjsSourceMapCache.get(Module._cache[key]);
    if (value) {
      obj[`file://${key}`] = value;
    }
  });
}

module.exports = {
  sourceMapCacheToObject,
  maybeCacheSourceMap
};
