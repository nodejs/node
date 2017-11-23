'use strict';

const fs = require('fs');
const internalCJSModule = require('internal/module');
const CJSModule = require('module');
const internalURLModule = require('internal/url');
const internalFS = require('internal/fs');
const NativeModule = require('native_module');
const { extname, _makeLong } = require('path');
const { URL } = require('url');
const { realpathSync } = require('fs');
const preserveSymlinks = !!process.binding('config').preserveSymlinks;
const {
  ModuleWrap,
  createDynamicModule
} = require('internal/loader/ModuleWrap');
const errors = require('internal/errors');

const search = require('internal/loader/search');
const asyncReadFile = require('util').promisify(require('fs').readFile);
const debug = require('util').debuglog('esm');

const realpathCache = new Map();

const loaders = new Map();
exports.loaders = loaders;

// Strategy for loading a standard JavaScript module
loaders.set('esm', async (url) => {
  const source = `${await asyncReadFile(new URL(url))}`;
  debug(`Loading StandardModule ${url}`);
  return {
    module: new ModuleWrap(internalCJSModule.stripShebang(source), url),
    reflect: undefined
  };
});

// Strategy for loading a node-style CommonJS module
const isWindows = process.platform === 'win32';
const winSepRegEx = /\//g;
loaders.set('cjs', async (url) => {
  const pathname = internalURLModule.getPathFromURL(new URL(url));
  const module = CJSModule._cache[
      isWindows ? pathname.replace(winSepRegEx, '\\') : pathname];
  if (module && module.loaded) {
    const ctx = createDynamicModule(['default'], url, undefined);
    ctx.reflect.exports.default.set(module.exports);
    return ctx;
  }
  return createDynamicModule(['default'], url, (reflect) => {
    debug(`Loading CJSModule ${url}`);
    CJSModule._load(pathname);
  });
});

// Strategy for loading a node builtin CommonJS module that isn't
// through normal resolution
loaders.set('builtin', async (url) => {
  return createDynamicModule(['default'], url, (reflect) => {
    debug(`Loading BuiltinModule ${url}`);
    const exports = NativeModule.require(url.substr(5));
    reflect.exports.default.set(exports);
  });
});

loaders.set('addon', async (url) => {
  const ctx = createDynamicModule(['default'], url, (reflect) => {
    debug(`Loading NativeModule ${url}`);
    const module = { exports: {} };
    const pathname = internalURLModule.getPathFromURL(new URL(url));
    process.dlopen(module, _makeLong(pathname));
    reflect.exports.default.set(module.exports);
  });
  return ctx;
});

loaders.set('json', async (url) => {
  return createDynamicModule(['default'], url, (reflect) => {
    debug(`Loading JSONModule ${url}`);
    const pathname = internalURLModule.getPathFromURL(new URL(url));
    const content = fs.readFileSync(pathname, 'utf8');
    try {
      const exports = JSON.parse(internalCJSModule.stripBOM(content));
      reflect.exports.default.set(exports);
    } catch (err) {
      err.message = pathname + ': ' + err.message;
      throw err;
    }
  });
});

exports.resolve = (specifier, parentURL) => {
  if (NativeModule.nonInternalExists(specifier)) {
    return {
      url: specifier,
      format: 'builtin'
    };
  }

  let url;
  try {
    url = search(specifier, parentURL);
  } catch (e) {
    if (e.message && e.message.startsWith('Cannot find module'))
      e.code = 'MODULE_NOT_FOUND';
    throw e;
  }

  if (url.protocol !== 'file:') {
    throw new errors.Error('ERR_INVALID_PROTOCOL',
                           url.protocol, 'file:');
  }

  if (!preserveSymlinks) {
    const real = realpathSync(internalURLModule.getPathFromURL(url), {
      [internalFS.realpathCacheKey]: realpathCache
    });
    const old = url;
    url = internalURLModule.getURLFromFilePath(real);
    url.search = old.search;
    url.hash = old.hash;
  }

  const ext = extname(url.pathname);
  switch (ext) {
    case '.mjs':
      return { url: `${url}`, format: 'esm' };
    case '.json':
      return { url: `${url}`, format: 'json' };
    case '.node':
      return { url: `${url}`, format: 'addon' };
    case '.js':
      return { url: `${url}`, format: 'cjs' };
    default:
      throw new errors.Error('ERR_UNKNOWN_FILE_EXTENSION',
                             internalURLModule.getPathFromURL(url));
  }
};
