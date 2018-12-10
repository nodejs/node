'use strict';

const {
  ERR_INVALID_RETURN_PROPERTY,
  ERR_INVALID_RETURN_PROPERTY_VALUE,
  ERR_INVALID_RETURN_VALUE,
  ERR_MISSING_DYNAMIC_INSTANTIATE_HOOK,
  ERR_UNKNOWN_MODULE_FORMAT
} = require('internal/errors').codes;
const { URL } = require('url');
const { validateString } = require('internal/validators');
const ModuleMap = require('internal/modules/esm/module_map');
const ModuleJob = require('internal/modules/esm/module_job');
const defaultResolve = require('internal/modules/esm/default_resolve');
const createDynamicModule = require(
  'internal/modules/esm/create_dynamic_module');
const translators = require('internal/modules/esm/translators');

const FunctionBind = Function.call.bind(Function.prototype.bind);

const debug = require('util').debuglog('esm');

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

    // The resolver has the signature
    //   (specifier : string, parentURL : string, defaultResolve)
    //       -> Promise<{ url : string, format: string }>
    // where defaultResolve is ModuleRequest.resolve (having the same
    // signature itself).
    // If `.format` on the returned value is 'dynamic', .dynamicInstantiate
    // will be used as described below.
    this._resolve = defaultResolve;
    // This hook is only called when resolve(...).format is 'dynamic' and
    // has the signature
    //   (url : string) -> Promise<{ exports: { ... }, execute: function }>
    // Where `exports` is an object whose property names define the exported
    // names of the generated module. `execute` is a function that receives
    // an object with the same keys as `exports`, whose values are get/set
    // functions for the actual exported values.
    this._dynamicInstantiate = undefined;
  }

  async resolve(specifier, parentURL) {
    const isMain = parentURL === undefined;
    if (!isMain)
      validateString(parentURL, 'parentURL');

    const resolved = await this._resolve(specifier, parentURL, defaultResolve);

    if (typeof resolved !== 'object')
      throw new ERR_INVALID_RETURN_VALUE(
        'object', 'loader resolve', resolved
      );

    const { url, format } = resolved;

    if (typeof url !== 'string')
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
        'string', 'loader resolve', 'url', url
      );

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

    if (format !== 'dynamic' && !url.startsWith('file:'))
      throw new ERR_INVALID_RETURN_PROPERTY(
        'file: url', 'loader resolve', 'url', url
      );

    return { url, format };
  }

  async import(specifier, parent) {
    const job = await this.getModuleJob(specifier, parent);
    const module = await job.run();
    return module.namespace();
  }

  hook({ resolve, dynamicInstantiate }) {
    // Use .bind() to avoid giving access to the Loader instance when called.
    if (resolve !== undefined)
      this._resolve = FunctionBind(resolve, null);
    if (dynamicInstantiate !== undefined)
      this._dynamicInstantiate = FunctionBind(dynamicInstantiate, null);
  }

  async getModuleJob(specifier, parentURL) {
    const { url, format } = await this.resolve(specifier, parentURL);
    let job = this.moduleMap.get(url);
    if (job !== undefined)
      return job;

    let loaderInstance;
    if (format === 'dynamic') {
      if (typeof this._dynamicInstantiate !== 'function')
        throw new ERR_MISSING_DYNAMIC_INSTANTIATE_HOOK();

      loaderInstance = async (url) => {
        debug(`Translating dynamic ${url}`);
        const { exports, execute } = await this._dynamicInstantiate(url);
        return createDynamicModule(exports, url, (reflect) => {
          debug(`Loading dynamic ${url}`);
          execute(reflect.exports);
        });
      };
    } else {
      if (!translators.has(format))
        throw new ERR_UNKNOWN_MODULE_FORMAT(format);

      loaderInstance = translators.get(format);
    }

    job = new ModuleJob(this, url, loaderInstance, parentURL === undefined);
    this.moduleMap.set(url, job);
    return job;
  }
}

Object.setPrototypeOf(Loader.prototype, null);

module.exports = Loader;
