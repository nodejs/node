'use strict';

const {
  SafeMap,
} = primordials;

const internalFS = require('internal/fs/utils');
const { NativeModule } = require('internal/bootstrap/loaders');
const { realpathSync } = require('fs');
const { getOptionValue } = require('internal/options');

const preserveSymlinks = getOptionValue('--preserve-symlinks');
const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
const typeFlag = getOptionValue('--input-type');
const { resolve: moduleWrapResolve } = internalBinding('module_wrap');
const { URL, pathToFileURL, fileURLToPath } = require('internal/url');
const { ERR_INPUT_TYPE_NOT_ALLOWED,
        ERR_UNSUPPORTED_ESM_URL_SCHEME } = require('internal/errors').codes;

const realpathCache = new SafeMap();

function defaultResolve(specifier, { parentURL } = {}, defaultResolve) {
  let parsed;
  try {
    parsed = new URL(specifier);
    if (parsed.protocol === 'data:') {
      return {
        url: specifier
      };
    }
  } catch {}
  if (parsed && parsed.protocol !== 'file:' && parsed.protocol !== 'data:')
    throw new ERR_UNSUPPORTED_ESM_URL_SCHEME();
  if (NativeModule.canBeRequiredByUsers(specifier)) {
    return {
      url: specifier
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

  return { url: `${url}` };
}
exports.defaultResolve = defaultResolve;
