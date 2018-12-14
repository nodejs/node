'use strict';

const internalFS = require('internal/fs/utils');
const { NativeModule } = require('internal/bootstrap/loaders');
const { extname } = require('path');
const { realpathSync } = require('fs');
const { getOptionValue } = require('internal/options');
const preserveSymlinks = getOptionValue('--preserve-symlinks');
const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
const { ERR_UNSUPPORTED_FILE_EXTENSION } = require('internal/errors').codes;
const { resolve: moduleWrapResolve } = internalBinding('module_wrap');
const { pathToFileURL, fileURLToPath } = require('internal/url');

const realpathCache = new Map();

const extensionFormatMap = {
  '__proto__': null,
  '.mjs': 'esm',
  '.js': 'esm'
};

const legacyExtensionFormatMap = {
  '__proto__': null,
  '.js': 'cjs',
  '.json': 'cjs',
  '.mjs': 'esm',
  '.node': 'cjs'
};

function resolve(specifier, parentURL) {
  if (NativeModule.canBeRequiredByUsers(specifier)) {
    return {
      url: specifier,
      format: 'builtin'
    };
  }

  const isMain = parentURL === undefined;
  if (isMain)
    parentURL = pathToFileURL(`${process.cwd()}/`).href;

  const resolved = moduleWrapResolve(specifier, parentURL, isMain);

  let url = resolved.url;
  const legacy = resolved.legacy;

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
  let format = (legacy ? legacyExtensionFormatMap : extensionFormatMap)[ext];

  if (!format) {
    if (isMain)
      format = legacy ? 'cjs' : 'esm';
    else
      throw new ERR_UNSUPPORTED_FILE_EXTENSION(fileURLToPath(url),
                                               fileURLToPath(parentURL));
  }

  return { url: `${url}`, format };
}

module.exports = resolve;
