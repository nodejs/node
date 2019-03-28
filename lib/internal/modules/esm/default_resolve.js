'use strict';

const internalFS = require('internal/fs/utils');
const { NativeModule } = require('internal/bootstrap/loaders');
const { extname } = require('path');
const { realpathSync } = require('fs');
const { getOptionValue } = require('internal/options');

const preserveSymlinks = getOptionValue('--preserve-symlinks');
const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
const experimentalJsonModules = getOptionValue('--experimental-json-modules');
const typeFlag = getOptionValue('--entry-type');

const { resolve: moduleWrapResolve,
        getPackageType } = internalBinding('module_wrap');
const { pathToFileURL, fileURLToPath } = require('internal/url');
const { ERR_ENTRY_TYPE_MISMATCH,
        ERR_UNKNOWN_FILE_EXTENSION } = require('internal/errors').codes;

const {
  Object,
  SafeMap
} = primordials;

const realpathCache = new SafeMap();

// const TYPE_NONE = 0;
const TYPE_COMMONJS = 1;
const TYPE_MODULE = 2;

const extensionFormatMap = {
  '__proto__': null,
  '.cjs': 'commonjs',
  '.js': 'module',
  '.mjs': 'module'
};

const legacyExtensionFormatMap = {
  '__proto__': null,
  '.cjs': 'commonjs',
  '.js': 'commonjs',
  '.json': 'commonjs',
  '.mjs': 'module',
  '.node': 'commonjs'
};

if (experimentalJsonModules) {
  // This is a total hack
  Object.assign(extensionFormatMap, {
    '.json': 'json'
  });
  Object.assign(legacyExtensionFormatMap, {
    '.json': 'json'
  });
}

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

  let url = moduleWrapResolve(specifier, parentURL);

  if (isMain ? !preserveSymlinksMain : !preserveSymlinks) {
    const real = realpathSync(fileURLToPath(url), {
      [internalFS.realpathCacheKey]: realpathCache
    });
    const old = url;
    url = pathToFileURL(real);
    url.search = old.search;
    url.hash = old.hash;
  }

  const type = getPackageType(url.href);

  const ext = extname(url.pathname);
  const extMap =
      type !== TYPE_MODULE ? legacyExtensionFormatMap : extensionFormatMap;
  let format = extMap[ext];

  if (isMain && typeFlag) {
    // Conflict between explicit extension (.mjs, .cjs) and --entry-type
    if (ext === '.cjs' && typeFlag === 'module' ||
        ext === '.mjs' && typeFlag === 'commonjs') {
      throw new ERR_ENTRY_TYPE_MISMATCH(
        fileURLToPath(url), ext, typeFlag, 'extension');
    }

    // Conflict between package scope type and --entry-type
    if (ext === '.js') {
      if (type === TYPE_MODULE && typeFlag === 'commonjs' ||
          type === TYPE_COMMONJS && typeFlag === 'module') {
        throw new ERR_ENTRY_TYPE_MISMATCH(
          fileURLToPath(url), ext, typeFlag, 'scope');
      }
    }
  }
  if (!format) {
    if (isMain && typeFlag)
      format = typeFlag;
    else if (isMain)
      format = type === TYPE_MODULE ? 'module' : 'commonjs';
    else
      throw new ERR_UNKNOWN_FILE_EXTENSION(fileURLToPath(url),
                                           fileURLToPath(parentURL));
  }
  return { url: `${url}`, format };
}

module.exports = resolve;
