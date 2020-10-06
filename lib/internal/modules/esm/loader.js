'use strict';

// This is needed to avoid cycles in esm/resolve <-> cjs/loader
require('internal/modules/cjs/loader');

const {
  FunctionPrototypeBind,
  ObjectSetPrototypeOf,
  SafeWeakMap,
} = primordials;

const {
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_RETURN_PROPERTY,
  ERR_INVALID_RETURN_PROPERTY_VALUE,
  ERR_INVALID_RETURN_VALUE,
  ERR_UNKNOWN_MODULE_FORMAT
} = require('internal/errors').codes;
const { URL, pathToFileURL } = require('internal/url');
const { validateString } = require('internal/validators');
const ModuleMap = require('internal/modules/esm/module_map');
const ModuleJob = require('internal/modules/esm/module_job');

const {
  defaultResolve,
  DEFAULT_CONDITIONS,
} = require('internal/modules/esm/resolve');
const { defaultGetSource } = require('internal/modules/esm/get_source');
const { translators } = require('internal/modules/esm/translators');
const { getOptionValue } = require('internal/options');

/* A Loader instance is used as the main entry point for loading ES modules.
 * Currently, this is a singleton -- there is only one used for loading
 * the main module and everything in its dependency graph. */
class Loader {
  constructor() {
    // Methods which translate input code or other information
    // into es modules
    this.translators = translators;

    // Registry of loaded modules, akin to `require.cache`
    this.moduleMap = new ModuleMap();

    // Map of already-loaded CJS modules to use
    this.cjsCache = new SafeWeakMap();

    // This hook is called before the first root module is imported. It's a
    // function that returns a piece of code that runs as a sloppy-mode script.
    // The script may evaluate to a function that can be called with a
    // `getBuiltin` helper that can be used to retrieve builtins.
    // If the hook returns `null` instead of a source string, it opts out of
    // running any preload code.
    // The preload code runs as soon as the hook module has finished evaluating.
    this._getGlobalPreloadCode = null;

    this._resolveToURL = defaultResolve;
    this._loadFromURL = defaultGetSource;

    // The index for assigning unique URLs to anonymous module evaluation
    this.evalIndex = 0;
  }

  async resolve(specifier, parentURL) {
    const isMain = parentURL === undefined;
    if (!isMain)
      validateString(parentURL, 'parentURL');

    const resolveResponse = await this._resolveToURL(
      specifier, { parentURL, conditions: DEFAULT_CONDITIONS }, defaultResolve);
    if (typeof resolveResponse !== 'object') {
      throw new ERR_INVALID_RETURN_VALUE(
        'object', 'loader resolve', resolveResponse);
    }

    const { url } = resolveResponse;
    if (typeof url !== 'string') {
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
        'string', 'loader resolve', 'url', url);
    }
    return url;
  }

  async eval(
    source,
    url = pathToFileURL(`${process.cwd()}/[eval${++this.evalIndex}]`).href
  ) {
    const evalInstance = (url) => {
      const { ModuleWrap, callbackMap } = internalBinding('module_wrap');
      const module = new ModuleWrap(url, undefined, source, 0, 0);
      callbackMap.set(module, {
        importModuleDynamically: (specifier, { url }) => {
          return this.import(specifier, url);
        }
      });

      return module;
    };
    const job = new ModuleJob(this, url, evalInstance, false, false);
    this.moduleMap.set(url, job);
    const { module } = await job.run();
    return {
      namespace: module.getNamespace(),
    };
  }

  async import(specifier, parent) {
    const job = await this.getModuleJob(specifier, parent);
    const { module } = await job.run();
    return module.getNamespace();
  }

  hook(hooks) {
    const {
      resolveToURL,
      loadFromURL,
      getGlobalPreloadCode,

      // previous hooks:
      dynamicInstantiate,
    } = hooks;

    if (dynamicInstantiate !== undefined) {
      process.emitWarning(
        'The dynamicInstantiate loader hook has been removed.');
    }

    // Use .bind() to avoid giving access to the Loader instance when called.
    if (resolveToURL !== undefined)
      this._resolveToURL = FunctionPrototypeBind(resolveToURL, null);
    if (loadFromURL !== undefined) {
      this._loadFromURL = FunctionPrototypeBind(loadFromURL, null);
    }
    if (getGlobalPreloadCode !== undefined) {
      this._getGlobalPreloadCode =
        FunctionPrototypeBind(getGlobalPreloadCode, null);
    }
  }

  runGlobalPreloadCode() {
    if (!this._getGlobalPreloadCode) {
      return;
    }
    const preloadCode = this._getGlobalPreloadCode();
    if (preloadCode === null) {
      return;
    }

    if (typeof preloadCode !== 'string') {
      throw new ERR_INVALID_RETURN_VALUE(
        'string', 'loader getGlobalPreloadCode', preloadCode);
    }
    const { compileFunction } = require('vm');
    const preloadInit = compileFunction(preloadCode, ['getBuiltin'], {
      filename: '<preload>',
    });
    const { NativeModule } = require('internal/bootstrap/loaders');

    preloadInit.call(globalThis, (builtinName) => {
      if (NativeModule.canBeRequiredByUsers(builtinName)) {
        return require(builtinName);
      }
      throw new ERR_INVALID_ARG_VALUE('builtinName', builtinName);
    });
  }

  async getModuleJob(specifier, parentURL) {
    const url = await this.resolve(specifier, parentURL);
    let job = this.moduleMap.get(url);
    // CommonJS will set functions for lazy job evaluation.
    if (typeof job === 'function')
      this.moduleMap.set(url, job = job());
    if (job !== undefined)
      return job;

    const moduleProvider = async (url, isMain) => {
      const { source, format } = await this._loadFromURL(url, {}, defaultGetSource);
      if (!translators.has(format))
        throw new ERR_UNKNOWN_MODULE_FORMAT(format);
      const translator = translators.get(format);
      return translator.call(this, url, source, isMain);
    }

    const inspectBrk = parentURL === undefined && getOptionValue('--inspect-brk');
    job = new ModuleJob(this, url, moduleProvider, parentURL === undefined,
                        inspectBrk);
    this.moduleMap.set(url, job);
    return job;
  }
}

ObjectSetPrototypeOf(Loader.prototype, null);

exports.Loader = Loader;
