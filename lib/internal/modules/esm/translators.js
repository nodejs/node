'use strict';

const { NativeModule } = require('internal/bootstrap/loaders');
const { ModuleWrap, callbackMap } = internalBinding('module_wrap');
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
const {
  SafeMap,
  JSON,
  FunctionPrototype,
  StringPrototype
} = primordials;
const { URL } = require('url');
const { debuglog, promisify } = require('util');
const esmLoader = require('internal/process/esm_loader');

const readFileAsync = promisify(fs.readFile);
const readFileSync = fs.readFileSync;
const StringReplace = FunctionPrototype.call.bind(StringPrototype.replace);
const JsonParse = JSON.parse;

const debug = debuglog('esm');

const translators = new SafeMap();
module.exports = translators;

function initializeImportMeta(meta, { url }) {
  meta.url = url;
}

async function importModuleDynamically(specifier, { url }) {
  const loader = await esmLoader.loaderPromise;
  return loader.import(specifier, url);
}

// Strategy for loading a standard JavaScript module
translators.set('esm', async (url) => {
  const source = `${await readFileAsync(new URL(url))}`;
  debug(`Translating StandardModule ${url}`);
  const module = new ModuleWrap(stripShebang(source), url);
  callbackMap.set(module, {
    initializeImportMeta,
    importModuleDynamically,
  });
  return {
    module,
    reflect: undefined,
  };
});

// Strategy for loading a node-style CommonJS module
const isWindows = process.platform === 'win32';
const winSepRegEx = /\//g;
translators.set('cjs', async (url, isMain) => {
  debug(`Translating CJSModule ${url}`);
  const pathname = internalURLModule.fileURLToPath(new URL(url));
  const module = CJSModule._cache[
    isWindows ? StringReplace(pathname, winSepRegEx, '\\') : pathname];
  if (module && module.loaded) {
    const exports = module.exports;
    return createDynamicModule(['default'], url, (reflect) => {
      reflect.exports.default.set(exports);
    });
  }
  return createDynamicModule(['default'], url, () => {
    debug(`Loading CJSModule ${url}`);
    // We don't care about the return val of _load here because Module#load
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
  const module = NativeModule.map.get(id);
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
    const pathname = internalURLModule.fileURLToPath(new URL(url));
    process.dlopen(module, _makeLong(pathname));
    reflect.exports.default.set(module.exports);
  });
});

// Strategy for loading a JSON file
translators.set('json', async (url) => {
  debug(`Translating JSONModule ${url}`);
  return createDynamicModule(['default'], url, (reflect) => {
    debug(`Loading JSONModule ${url}`);
    const pathname = internalURLModule.fileURLToPath(new URL(url));
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
