'use strict';

// This is needed to avoid cycles in esm/resolve <-> cjs/loader
require('internal/modules/cjs/loader');

const {
  FunctionPrototypeBind,
  ObjectSetPrototypeOf,
  RegExpPrototypeExec,
  SafeWeakMap,
  StringPrototypeStartsWith,
  globalThis,
} = primordials;

const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_MODULE_SPECIFIER,
  ERR_INVALID_RETURN_PROPERTY,
  ERR_INVALID_RETURN_PROPERTY_VALUE,
  ERR_INVALID_RETURN_VALUE,
  ERR_UNKNOWN_MODULE_FORMAT
} = require('internal/errors').codes;
const { URL, pathToFileURL, isURLInstance } = require('internal/url');
const ModuleMap = require('internal/modules/esm/module_map');
const ModuleJob = require('internal/modules/esm/module_job');

const {
  defaultResolve,
  DEFAULT_CONDITIONS,
} = require('internal/modules/esm/resolve');
const { defaultGetFormat } = require('internal/modules/esm/get_format');
const { defaultGetSource } = require(
  'internal/modules/esm/get_source');
const { defaultTransformSource } = require(
  'internal/modules/esm/transform_source');
const { translators } = require(
  'internal/modules/esm/translators');
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
    // The resolver has the signature
    //   (specifier : string, parentURL : string, defaultResolve)
    //       -> Promise<{ url : string }>
    // where defaultResolve is ModuleRequest.resolve (having the same
    // signature itself).
    this._resolve = defaultResolve;
    // This hook is called after the module is resolved but before a translator
    // is chosen to load it; the format returned by this function is the name
    // of a translator.
    this._getFormat = defaultGetFormat;
    // This hook is called just before the source code of an ES module file
    // is loaded.
    this._getSource = defaultGetSource;
    // This hook is called just after the source code of an ES module file
    // is loaded, but before anything is done with the string.
    this._transformSource = defaultTransformSource;
    // The index for assigning unique URLs to anonymous module evaluation
    this.evalIndex = 0;
  }

  async resolve(specifier, parentURL) {
    const isMain = parentURL === undefined;
    if (!isMain && typeof parentURL !== 'string' && !isURLInstance(parentURL))
      throw new ERR_INVALID_ARG_TYPE('parentURL', ['string', 'URL'], parentURL);

    const resolveResponse = await this._resolve(
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

  async getFormat(url) {
    const getFormatResponse = await this._getFormat(
      url, {}, defaultGetFormat);
    if (typeof getFormatResponse !== 'object') {
      throw new ERR_INVALID_RETURN_VALUE(
        'object', 'loader getFormat', getFormatResponse);
    }

    const { format } = getFormatResponse;
    if (format === null) {
      const dataUrl = RegExpPrototypeExec(
        /^data:([^/]+\/[^;,]+)(?:[^,]*?)(;base64)?,/,
        url,
      );
      throw new ERR_INVALID_MODULE_SPECIFIER(
        url,
        dataUrl ? `has an unsupported MIME type "${dataUrl[1]}"` : '');
    }
    if (typeof format !== 'string') {
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
        'string', 'loader getFormat', 'format', format);
    }

    if (format === 'builtin') {
      return format;
    }

    if (this._resolve !== defaultResolve) {
      try {
        new URL(url);
      } catch {
        throw new ERR_INVALID_RETURN_PROPERTY(
          'url', 'loader resolve', 'url', url
        );
      }
    }

    if (this._resolve === defaultResolve &&
      !StringPrototypeStartsWith(url, 'file:') &&
      !StringPrototypeStartsWith(url, 'data:')
    ) {
      throw new ERR_INVALID_RETURN_PROPERTY(
        'file: or data: url', 'loader resolve', 'url', url
      );
    }

    return format;
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
      resolve,
      dynamicInstantiate,
      getFormat,
      getSource,
      transformSource,
      getGlobalPreloadCode,
    } = hooks;

    // Use .bind() to avoid giving access to the Loader instance when called.
    if (resolve !== undefined)
      this._resolve = FunctionPrototypeBind(resolve, null);
    if (dynamicInstantiate !== undefined) {
      process.emitWarning(
        'The dynamicInstantiate loader hook has been removed.');
    }
    if (getFormat !== undefined) {
      this._getFormat = FunctionPrototypeBind(getFormat, null);
    }
    if (getSource !== undefined) {
      this._getSource = FunctionPrototypeBind(getSource, null);
    }
    if (transformSource !== undefined) {
      this._transformSource = FunctionPrototypeBind(transformSource, null);
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
    const format = await this.getFormat(url);
    let job = this.moduleMap.get(url);
    // CommonJS will set functions for lazy job evaluation.
    if (typeof job === 'function')
      this.moduleMap.set(url, job = job());
    if (job !== undefined)
      return job;

    if (!translators.has(format))
      throw new ERR_UNKNOWN_MODULE_FORMAT(format);

    const loaderInstance = translators.get(format);

    const inspectBrk = parentURL === undefined &&
        format === 'module' && getOptionValue('--inspect-brk');
    job = new ModuleJob(this, url, loaderInstance, parentURL === undefined,
                        inspectBrk);
    this.moduleMap.set(url, job);
    return job;
  }
}

ObjectSetPrototypeOf(Loader.prototype, null);

exports.Loader = Loader;
