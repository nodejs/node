'use strict';

const path = require('path');
const { getURLFromFilePath, URL } = require('internal/url');
const errors = require('internal/errors');

const ModuleMap = require('internal/loader/ModuleMap');
const ModuleJob = require('internal/loader/ModuleJob');
const defaultResolve = require('internal/loader/DefaultResolve');
const createDynamicModule = require('internal/loader/CreateDynamicModule');
const translators = require('internal/loader/Translators');
const { setImportModuleDynamicallyCallback } = internalBinding('module_wrap');
const FunctionBind = Function.call.bind(Function.prototype.bind);

const debug = require('util').debuglog('esm');

// Returns a file URL for the current working directory.
function getURLStringForCwd() {
  try {
    return getURLFromFilePath(`${process.cwd()}/`).href;
  } catch (e) {
    e.stack;
    // If the current working directory no longer exists.
    if (e.code === 'ENOENT') {
      return undefined;
    }
    throw e;
  }
}

function normalizeReferrerURL(referrer) {
  if (typeof referrer === 'string' && path.isAbsolute(referrer)) {
    return getURLFromFilePath(referrer).href;
  }
  return new URL(referrer).href;
}

/* A Loader instance is used as the main entry point for loading ES modules.
 * Currently, this is a singleton -- there is only one used for loading
 * the main module and everything in its dependency graph. */
class Loader {
  constructor(base = getURLStringForCwd()) {
    if (typeof base !== 'string')
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'base', 'string');

    this.base = base;
    this.isMain = true;

    // methods which translate input code or other information
    // into es modules
    this.translators = translators;

    // registry of loaded modules, akin to `require.cache`
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

  async resolve(specifier, parentURL = this.base) {
    if (typeof parentURL !== 'string')
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'parentURL', 'string');

    const { url, format } =
      await this._resolve(specifier, parentURL, defaultResolve);

    if (typeof url !== 'string')
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'url', 'string');

    if (typeof format !== 'string')
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'format', 'string');

    if (format === 'builtin')
      return { url: `node:${url}`, format };

    if (format !== 'dynamic' && !url.startsWith('file:'))
      throw new errors.Error('ERR_INVALID_PROTOCOL', url, 'file:');

    return { url, format };
  }

  async import(specifier, parent = this.base) {
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

  async getModuleJob(specifier, parentURL = this.base) {
    const { url, format } = await this.resolve(specifier, parentURL);
    let job = this.moduleMap.get(url);
    if (job !== undefined)
      return job;

    let loaderInstance;
    if (format === 'dynamic') {
      if (typeof this._dynamicInstantiate !== 'function')
        throw new errors.Error('ERR_MISSING_DYNAMIC_INTSTANTIATE_HOOK');

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
        throw new errors.RangeError('ERR_UNKNOWN_MODULE_FORMAT', format);

      loaderInstance = translators.get(format);
    }

    let inspectBrk = false;
    if (this.isMain) {
      if (process._breakFirstLine) {
        delete process._breakFirstLine;
        inspectBrk = true;
      }
      this.isMain = false;
    }
    job = new ModuleJob(this, url, loaderInstance, inspectBrk);
    this.moduleMap.set(url, job);
    return job;
  }

  static registerImportDynamicallyCallback(loader) {
    setImportModuleDynamicallyCallback(async (referrer, specifier) => {
      return loader.import(specifier, normalizeReferrerURL(referrer));
    });
  }
}

Object.setPrototypeOf(Loader.prototype, null);
module.exports = Loader;
