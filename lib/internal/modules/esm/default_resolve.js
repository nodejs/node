'use strict';

const {
  SafeMap,
} = primordials;

const internalFS = require('internal/fs/utils');
const { NativeModule } = require('internal/bootstrap/loaders');
const { extname } = require('path');
const { realpathSync } = require('fs');
const { getOptionValue } = require('internal/options');

const preserveSymlinks = getOptionValue('--preserve-symlinks');
const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
const experimentalJsonModules = getOptionValue('--experimental-json-modules');
const esModuleSpecifierResolution =
  getOptionValue('--es-module-specifier-resolution');
const typeFlag = getOptionValue('--input-type');
const experimentalWasmModules = getOptionValue('--experimental-wasm-modules');
const { resolve: moduleWrapResolve,
        getPackageType } = internalBinding('module_wrap');
const { URL, pathToFileURL, fileURLToPath } = require('internal/url');
const { ERR_INPUT_TYPE_NOT_ALLOWED,
        ERR_UNKNOWN_FILE_EXTENSION } = require('internal/errors').codes;

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

if (experimentalWasmModules)
  extensionFormatMap['.wasm'] = legacyExtensionFormatMap['.wasm'] = 'wasm';

if (experimentalJsonModules)
  extensionFormatMap['.json'] = legacyExtensionFormatMap['.json'] = 'json';

function resolve(specifier, parentURL) {
  try {
    const parsed = new URL(specifier);
    if (parsed.protocol === 'data:') {
      const [ , mime ] = /^([^/]+\/[^;,]+)(?:[^,]*?)(;base64)?,/.exec(parsed.pathname) || [ null, null, null ];
      const format = ({
        '__proto__': null,
        'text/javascript': 'module',
        'application/json': 'json',
        'application/wasm': experimentalWasmModules ? 'wasm' : null
      })[mime] || null;
      return {
        url: specifier,
        format
      };
    }
  } catch {}
  if (NativeModule.canBeRequiredByUsers(specifier)) {
    return {
      url: specifier,
      format: 'builtin'
    };
  }
  if (parentURL && parentURL.startsWith('data:')) {
    // This is gonna blow up, we want the error
    new URL(specifier, parentURL);
  }

  const isMain = parentURL === undefined;
  if (isMain) {
    parentURL = pathToFileURL(`${process.cwd()}/`).href;

    // This is the initial entry point to the program, and --input-type has
    // been passed as an option; but --input-type can only be used with
    // --eval, --print or STDIN string input. It is not allowed with file
    // input, to avoid user confusion over how expansive the effect of the
    // flag should be (i.e. entry point only, package scope surrounding the
    // entry point, etc.).
    if (typeFlag)
      throw new ERR_INPUT_TYPE_NOT_ALLOWED();
  }

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

  const ext = extname(url.pathname);
  let format = extensionFormatMap[ext];
  if (ext === '.js' || (!format && isMain))
    format = getPackageType(url.href) === TYPE_MODULE ? 'module' : 'commonjs';
  if (!format) {
    if (esModuleSpecifierResolution === 'node')
      format = legacyExtensionFormatMap[ext];
    else
      throw new ERR_UNKNOWN_FILE_EXTENSION(fileURLToPath(url));
  }
  return { url: `${url}`, format };
}

module.exports = resolve;
