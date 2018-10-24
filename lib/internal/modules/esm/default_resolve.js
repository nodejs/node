'use strict';

const { URL } = require('url');
const CJSmodule = require('internal/modules/cjs/loader');
const internalFS = require('internal/fs/utils');
const { NativeModule } = require('internal/bootstrap/loaders');
const { extname } = require('path');
const { realpathSync } = require('fs');
const { getOptionValue } = require('internal/options');
const preserveSymlinks = getOptionValue('--preserve-symlinks');
const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
const {
  ERR_MODULE_NOT_FOUND,
  ERR_UNKNOWN_FILE_EXTENSION
} = require('internal/errors').codes;
const { resolve: moduleWrapResolve } = internalBinding('module_wrap');
const { pathToFileURL, fileURLToPath } = require('internal/url');

const realpathCache = new Map();

function search(target, base) {
  try {
    return moduleWrapResolve(target, base);
  } catch (e) {
    e.stack; // cause V8 to generate stack before rethrow
    let error = e;
    try {
      const questionedBase = new URL(base);
      const tmpMod = new CJSmodule(questionedBase.pathname, null);
      tmpMod.paths = CJSmodule._nodeModulePaths(
        new URL('./', questionedBase).pathname);
      const found = CJSmodule._resolveFilename(target, tmpMod);
      error = new ERR_MODULE_NOT_FOUND(target, fileURLToPath(base), found);
    } catch {
      // ignore
    }
    throw error;
  }
}

const extensionFormatMap = {
  '__proto__': null,
  '.mjs': 'esm'
};

function resolve(specifier, parentURL) {
  if (NativeModule.canBeRequiredByUsers(specifier)) {
    return {
      url: specifier,
      format: 'builtin'
    };
  }

  let url = search(specifier,
                   parentURL || pathToFileURL(`${process.cwd()}/`).href);

  const isMain = parentURL === undefined;

  if (isMain ? !preserveSymlinksMain : !preserveSymlinks) {
    const real = realpathSync(fileURLToPath(url), {
      [internalFS.realpathCacheKey]: realpathCache
    });
    const old = url;
    url = pathToFileURL(real);
    url.search = old.search;
    url.hash = old.hash;
  }

  const ext = extname(url.pathname);

  let format = extensionFormatMap[ext];
  if (!format) {
    if (isMain)
      format = 'cjs';
    else
      throw new ERR_UNKNOWN_FILE_EXTENSION(url.pathname,
                                           fileURLToPath(parentURL));
  }

  return { url: `${url}`, format };
}

module.exports = resolve;
