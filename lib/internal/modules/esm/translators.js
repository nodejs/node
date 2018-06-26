'use strict';

const { NativeModule, internalBinding } = require('internal/bootstrap/loaders');
const { ModuleWrap } = internalBinding('module_wrap');
const {
  stripShebang,
  stripBOM
} = require('internal/modules/cjs/helpers');
const CJSModule = require('internal/modules/cjs/loader');
const internalURLModule = require('internal/url');
const createDynamicModule = require(
  'internal/modules/esm/create_dynamic_module');
const fs = require('fs');
const { _makeLong } = require('path');
const { SafeMap } = require('internal/safe_globals');
const { URL } = require('url');
const debug = require('util').debuglog('esm');
const readFileAsync = require('util').promisify(fs.readFile);
const readFileSync = fs.readFileSync;
const StringReplace = Function.call.bind(String.prototype.replace);
const JsonParse = JSON.parse;

const translators = new SafeMap();
module.exports = translators;

// Strategy for loading a standard JavaScript module
translators.set('esm', async (url) => {
  const source = `${await readFileAsync(new URL(url))}`;
  debug(`Translating StandardModule ${url}`);
  return {
    module: new ModuleWrap(stripShebang(source), url),
    reflect: undefined
  };
});

// Strategy for loading a node-style CommonJS module
const isWindows = process.platform === 'win32';
const winSepRegEx = /\//g;
translators.set('cjs', async (url, isMain) => {
  debug(`Translating CJSModule ${url}`);
  const pathname = internalURLModule.getPathFromURL(new URL(url));
  const module = CJSModule._cache[
    isWindows ? StringReplace(pathname, winSepRegEx, '\\') : pathname];
  if (module && module.loaded) {
    const ctx = createDynamicModule(['default'], url);
    ctx.reflect.exports.default.set(module.exports);
    return ctx;
  }
  return createDynamicModule(['default'], url, () => {
    debug(`Loading CJSModule ${url}`);
    // we don't care about the return val of _load here because Module#load
    // will handle it for us by checking the loader registry and filling the
    // exports like above
    CJSModule._load(pathname, undefined, isMain);
  });
});

// Strategy for loading a node builtin CommonJS module that isn't
// through normal resolution
translators.set('builtin', async (url) => {
  debug(`Translating BuiltinModule ${url}`);
  // slice 'node:' scheme
  const id = url.slice(5);
  NativeModule.require(id);
  const module = NativeModule.getCached(id);
  return createDynamicModule(
    [...module.exportKeys, 'default'], url, (reflect) => {
      debug(`Loading BuiltinModule ${url}`);
      module.reflect = reflect;
      for (const key of module.exportKeys)
        reflect.exports[key].set(module.exports[key]);
      reflect.exports.default.set(module.exports);
    });
});

// Strategy for loading a node native module
translators.set('addon', async (url) => {
  debug(`Translating NativeModule ${url}`);
  return createDynamicModule(['default'], url, (reflect) => {
    debug(`Loading NativeModule ${url}`);
    const module = { exports: {} };
    const pathname = internalURLModule.getPathFromURL(new URL(url));
    process.dlopen(module, _makeLong(pathname));
    reflect.exports.default.set(module.exports);
  });
});

// Strategy for loading a JSON file
translators.set('json', async (url) => {
  debug(`Translating JSONModule ${url}`);
  return createDynamicModule(['default'], url, (reflect) => {
    debug(`Loading JSONModule ${url}`);
    const pathname = internalURLModule.getPathFromURL(new URL(url));
    const content = readFileSync(pathname, 'utf8');
    try {
      const exports = JsonParse(stripBOM(content));
      reflect.exports.default.set(exports);
    } catch (err) {
      err.message = pathname + ': ' + err.message;
      throw err;
    }
  });
});
