'use strict';

const {
  FunctionPrototypeBind,
  ObjectSetPrototypeOf,
  SafeMap,
} = primordials;

const {
  ERR_INVALID_RETURN_PROPERTY,
  ERR_INVALID_RETURN_PROPERTY_VALUE,
  ERR_INVALID_RETURN_VALUE,
  ERR_MISSING_DYNAMIC_INSTANTIATE_HOOK,
  ERR_UNKNOWN_FILE_EXTENSION,
  ERR_UNKNOWN_MODULE_FORMAT
} = require('internal/errors').codes;
const { extname } = require('path');
const { URL, pathToFileURL, fileURLToPath } = require('internal/url');
const { validateString } = require('internal/validators');
const ModuleMap = require('internal/modules/esm/module_map');
const ModuleJob = require('internal/modules/esm/module_job');

const {
  defaultResolve,
  defaultGetFormat
} = require('internal/modules/esm/default_resolve');
const createDynamicModule = require(
  'internal/modules/esm/create_dynamic_module');
const { defaultGetSource, defaultTransformSource, translators } = require(
  'internal/modules/esm/translators');
const { getOptionValue } = require('internal/options');

const debug = require('internal/util/debuglog').debuglog('esm');

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
    this.cjsCache = new SafeMap();

    // The resolver has the signature
    //   (specifier : string, parentURL : string, defaultResolve)
    //       -> Promise<{ url : string, format: string }>
    // where defaultResolve is ModuleRequest.resolve (having the same
    // signature itself).
    // If `.format` on the returned value is 'dynamic', .dynamicInstantiate
    // will be used as described below.
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
    // This hook is only called when resolve(...).format is 'dynamic' and
    // has the signature
    //   (url : string) -> Promise<{ exports: { ... }, execute: function }>
    // Where `exports` is an object whose property names define the exported
    // names of the generated module. `execute` is a function that receives
    // an object with the same keys as `exports`, whose values are get/set
    // functions for the actual exported values.
    this._dynamicInstantiate = undefined;
    // The index for assigning unique URLs to anonymous module evaluation
    this.evalIndex = 0;
  }

  async resolve(specifier, parentURL) {
    const isMain = parentURL === undefined;
    if (!isMain)
      validateString(parentURL, 'parentURL');

    const resolved = await this._resolve(
      { specifier, parentURL }, defaultResolve, this);

    if (typeof resolved !== 'object')
      throw new ERR_INVALID_RETURN_VALUE(
        'object', 'loader resolve', resolved
      );

    const { url, format } = resolved;
    if (typeof url !== 'string')
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
        'string', 'loader resolve', 'url', url
      );
    if (!format && new URL(url).protocol === 'file:') {
      throw new ERR_UNKNOWN_FILE_EXTENSION(
        extname(new URL(url).pathname),
        fileURLToPath(url));
    }

    if (typeof format !== 'string')
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
        'string', 'loader resolve', 'format', format
      );

    if (format === 'builtin')
      return { url: `node:${url}`, format };

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
      format !== 'dynamic' &&
      !url.startsWith('file:') &&
      !url.startsWith('data:')
    )
      throw new ERR_INVALID_RETURN_PROPERTY(
        'file: or data: url', 'loader resolve', 'url', url
      );

    return { url, format };
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
    const { module, result } = await job.run();
    return {
      namespace: module.getNamespace(),
      result
    };
  }

  async import(specifier, parent) {
    const job = await this.getModuleJob(specifier, parent);
    const { module } = await job.run();
    return module.getNamespace();
  }

  hook({ resolve, dynamicInstantiate, getFormat, getSource, transformSource }) {
    // Use .bind() to avoid giving access to the Loader instance when called.
    if (resolve !== undefined)
      this._resolve = FunctionPrototypeBind(resolve, null);
    if (dynamicInstantiate !== undefined) {
      this._dynamicInstantiate =
        FunctionPrototypeBind(dynamicInstantiate, null);
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
  }

  async getModuleJob(specifier, parentURL) {
    const { url, format } = await this.resolve(specifier, parentURL);
    let job = this.moduleMap.get(url);
    // CommonJS will set functions for lazy job evaluation.
    if (typeof job === 'function')
      this.moduleMap.set(url, job = job());
    if (job !== undefined)
      return job;

    let loaderInstance;
    if (format === 'dynamic') {
      if (typeof this._dynamicInstantiate !== 'function')
        throw new ERR_MISSING_DYNAMIC_INSTANTIATE_HOOK();

      loaderInstance = async (url) => {
        debug(`Translating dynamic ${url}`);
        const { exports, execute } = await this._dynamicInstantiate(url);
        return createDynamicModule([], exports, url, (reflect) => {
          debug(`Loading dynamic ${url}`);
          execute(reflect.exports);
        }).module;
      };
    } else {
      if (!translators.has(format))
        throw new ERR_UNKNOWN_MODULE_FORMAT(format);

      loaderInstance = translators.get(format);
    }

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
