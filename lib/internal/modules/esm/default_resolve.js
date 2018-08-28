'use strict';

const internalFS = require('internal/fs/utils');
const { NativeModule } = require('internal/bootstrap/loaders');
const { extname } = require('path');
const { realpathSync, readFileSync } = require('fs');
const { getOptionValue } = require('internal/options');
const preserveSymlinks = getOptionValue('--preserve-symlinks');
const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
const { ERR_INVALID_PACKAGE_CONFIG,
        ERR_TYPE_MISMATCH,
        ERR_UNKNOWN_FILE_EXTENSION } = require('internal/errors').codes;
const { resolve: moduleWrapResolve } = internalBinding('module_wrap');
const { pathToFileURL, fileURLToPath, URL } = require('internal/url');
const asyncESM = require('internal/process/esm_loader');

const realpathCache = new Map();
// TOOD(@guybedford): Shared cache with C++
const pjsonCache = new Map();

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

function readPackageConfig(path, parentURL) {
  const existing = pjsonCache.get(path);
  if (existing !== undefined)
    return existing;
  try {
    return JSON.parse(readFileSync(path).toString());
  } catch (e) {
    if (e.code === 'ENOENT') {
      pjsonCache.set(path, null);
      return null;
    } else if (e instanceof SyntaxError) {
      throw new ERR_INVALID_PACKAGE_CONFIG(path, fileURLToPath(parentURL));
    }
    throw e;
  }
}

function getPackageBoundaryConfig(url, parentURL) {
  let pjsonURL = new URL('package.json', url);
  while (true) {
    const pcfg = readPackageConfig(fileURLToPath(pjsonURL), parentURL);
    if (pcfg)
      return pcfg;

    const lastPjsonURL = pjsonURL;
    pjsonURL = new URL('../package.json', pjsonURL);

    // Terminates at root where ../package.json equals ../../package.json
    // (can't just check "/package.json" for Windows support).
    if (pjsonURL.pathname === lastPjsonURL.pathname)
      return;
  }
}

function getModuleFormat(url, isMain, parentURL) {
  const pcfg = getPackageBoundaryConfig(url, parentURL);

  const legacy = !pcfg || pcfg.type !== 'module';

  const ext = extname(url.pathname);

  let format = (legacy ? legacyExtensionFormatMap : extensionFormatMap)[ext];

  if (!format) {
    if (isMain)
      format = legacy ? 'commonjs' : 'module';
    else
      throw new ERR_UNKNOWN_FILE_EXTENSION(fileURLToPath(url),
                                           fileURLToPath(parentURL));
  }

  // Check for mismatch between --type and file extension,
  // and between --type and the "type" field in package.json.
  if (isMain && format !== 'module' && asyncESM.typeFlag === 'module') {
    // Conflict between package scope type and --type
    if (ext === '.js') {
      if (pcfg && pcfg.type)
        throw new ERR_TYPE_MISMATCH(
          fileURLToPath(url), ext, asyncESM.typeFlag, 'scope');
    // Conflict between explicit extension (.mjs, .cjs) and --type
    } else {
      throw new ERR_TYPE_MISMATCH(
        fileURLToPath(url), ext, asyncESM.typeFlag, 'extension');
    }
  }

  return format;
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

  const format = getModuleFormat(url, isMain, parentURL);

  return { url: `${url}`, format };
}

module.exports = resolve;
