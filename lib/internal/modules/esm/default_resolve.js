'use strict';

const internalFS = require('internal/fs/utils');
const { NativeModule } = require('internal/bootstrap/loaders');
const { extname } = require('path');
const { realpathSync } = require('fs');
const { getOptionValue } = require('internal/options');

const preserveSymlinks = getOptionValue('--preserve-symlinks');
const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
const experimentalJsonModules = getOptionValue('--experimental-json-modules');
const typeFlag = getOptionValue('--input-type');

const { resolve: moduleWrapResolve,
        getPackageType } = internalBinding('module_wrap');
const { pathToFileURL, fileURLToPath } = require('internal/url');
const { ERR_INPUT_TYPE_NOT_ALLOWED,
        ERR_UNKNOWN_FILE_EXTENSION } = require('internal/errors').codes;

const {
  Object,
  SafeMap
} = primordials;

const realpathCache = new SafeMap();

// const TYPE_NONE = 0;
// const TYPE_COMMONJS = 1;
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
    // This is the initial entry point to the program, and --input-type has
    // been passed as an option; but --input-type can only be used with
    // --eval, --print or STDIN string input. It is not allowed with file
    // input, to avoid user confusion over how expansive the effect of the
    // flag should be (i.e. entry point only, package scope surrounding the
    // entry point, etc.).
    throw new ERR_INPUT_TYPE_NOT_ALLOWED();
  }
  if (!format) {
    if (isMain)
      format = type === TYPE_MODULE ? 'module' : 'commonjs';
    else
      throw new ERR_UNKNOWN_FILE_EXTENSION(fileURLToPath(url),
                                           fileURLToPath(parentURL));
  }
  return { url: `${url}`, format };
}

module.exports = resolve;
