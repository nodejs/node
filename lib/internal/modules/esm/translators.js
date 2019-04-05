'use strict';

const {
  SafeMap,
  StringPrototype,
  JSON
} = primordials;

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
const { fileURLToPath, URL } = require('url');
const { debuglog } = require('internal/util/debuglog');
const { promisify } = require('internal/util');
const esmLoader = require('internal/process/esm_loader');
const {
  ERR_UNKNOWN_BUILTIN_MODULE
} = require('internal/errors').codes;
const readFileAsync = promisify(fs.readFile);
const JsonParse = JSON.parse;

const debug = debuglog('esm');

const translators = new SafeMap();
exports.translators = translators;

function initializeImportMeta(meta, { url }) {
  meta.url = url;
}

async function importModuleDynamically(specifier, { url }) {
  const loader = await esmLoader.loaderPromise;
  return loader.import(specifier, url);
}

// Strategy for loading a standard JavaScript module
translators.set('module', async function moduleStrategy(url) {
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
translators.set('commonjs', async function commonjsStrategy(url, isMain) {
  debug(`Translating CJSModule ${url}`);
  const pathname = internalURLModule.fileURLToPath(new URL(url));
  const cached = this.cjsCache.get(url);
  if (cached) {
    this.cjsCache.delete(url);
    return cached;
  }
  const module = CJSModule._cache[
    isWindows ? StringPrototype.replace(pathname, winSepRegEx, '\\') : pathname
  ];
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
translators.set('builtin', async function builtinStrategy(url) {
  debug(`Translating BuiltinModule ${url}`);
  // Slice 'node:' scheme
  const id = url.slice(5);
  const module = NativeModule.map.get(id);
  if (!module) {
    throw new ERR_UNKNOWN_BUILTIN_MODULE(id);
  }
  module.compileForPublicLoader(true);
  return createDynamicModule(
    [...module.exportKeys, 'default'], url, (reflect) => {
      debug(`Loading BuiltinModule ${url}`);
      module.reflect = reflect;
      for (const key of module.exportKeys)
        reflect.exports[key].set(module.exports[key]);
      reflect.exports.default.set(module.exports);
    });
});

// Strategy for loading a JSON file
translators.set('json', async function jsonStrategy(url) {
  debug(`Translating JSONModule ${url}`);
  debug(`Loading JSONModule ${url}`);
  const pathname = fileURLToPath(url);
  const modulePath = isWindows ?
    StringPrototype.replace(pathname, winSepRegEx, '\\') : pathname;
  let module = CJSModule._cache[modulePath];
  if (module && module.loaded) {
    const exports = module.exports;
    return createDynamicModule(['default'], url, (reflect) => {
      reflect.exports.default.set(exports);
    });
  }
  const content = await readFileAsync(pathname, 'utf-8');
  try {
    const exports = JsonParse(stripBOM(content));
    module = {
      exports,
      loaded: true
    };
  } catch (err) {
    // TODO (BridgeAR): We could add a NodeCore error that wraps the JSON
    // parse error instead of just manipulating the original error message.
    // That would allow to add further properties and maybe additional
    // debugging information.
    err.message = pathname + ': ' + err.message;
    throw err;
  }
  CJSModule._cache[modulePath] = module;
  return createDynamicModule(['default'], url, (reflect) => {
    debug(`Parsing JSONModule ${url}`);
    reflect.exports.default.set(module.exports);
  });
});
