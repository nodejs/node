'use strict';

const { getURLFromFilePath } = require('internal/url');

const { createDynamicModule } = require('internal/loader/ModuleWrap');

const ModuleMap = require('internal/loader/ModuleMap');
const ModuleJob = require('internal/loader/ModuleJob');
const ModuleRequest = require('internal/loader/ModuleRequest');
const errors = require('internal/errors');
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

/* A Loader instance is used as the main entry point for loading ES modules.
 * Currently, this is a singleton -- there is only one used for loading
 * the main module and everything in its dependency graph. */
class Loader {
  constructor(base = getURLStringForCwd()) {
    if (typeof base !== 'string') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'base', 'string');
    }

    this.moduleMap = new ModuleMap();
    this.base = base;
    // The resolver has the signature
    //   (specifier : string, parentURL : string, defaultResolve)
    //       -> Promise<{ url : string,
    //                    format: anything in Loader.validFormats }>
    // where defaultResolve is ModuleRequest.resolve (having the same
    // signature itself).
    // If `.format` on the returned value is 'dynamic', .dynamicInstantiate
    // will be used as described below.
    this.resolver = ModuleRequest.resolve;
    // This hook is only called when resolve(...).format is 'dynamic' and has
    // the signature
    //   (url : string) -> Promise<{ exports: { ... }, execute: function }>
    // Where `exports` is an object whose property names define the exported
    // names of the generated module. `execute` is a function that receives
    // an object with the same keys as `exports`, whose values are get/set
    // functions for the actual exported values.
    this.dynamicInstantiate = undefined;
  }

  hook({ resolve = ModuleRequest.resolve, dynamicInstantiate }) {
    // Use .bind() to avoid giving access to the Loader instance when it is
    // called as this.resolver(...);
    this.resolver = resolve.bind(null);
    this.dynamicInstantiate = dynamicInstantiate;
  }

  // Typechecking wrapper around .resolver().
  async resolve(specifier, parentURL = this.base) {
    if (typeof parentURL !== 'string') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                                 'parentURL', 'string');
    }

    const { url, format } = await this.resolver(specifier, parentURL,
                                                ModuleRequest.resolve);

    if (!Loader.validFormats.includes(format)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'format',
                                 Loader.validFormats);
    }

    if (typeof url !== 'string') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'url', 'string');
    }

    if (format === 'builtin') {
      return { url: `node:${url}`, format };
    }

    if (format !== 'dynamic') {
      if (!ModuleRequest.loaders.has(format)) {
        throw new errors.Error('ERR_UNKNOWN_MODULE_FORMAT', format);
      }
      if (!url.startsWith('file:')) {
        throw new errors.Error('ERR_INVALID_PROTOCOL', url, 'file:');
      }
    }

    return { url, format };
  }

  // May create a new ModuleJob instance if one did not already exist.
  async getModuleJob(specifier, parentURL = this.base) {
    const { url, format } = await this.resolve(specifier, parentURL);
    let job = this.moduleMap.get(url);
    if (job === undefined) {
      let loaderInstance;
      if (format === 'dynamic') {
        const { dynamicInstantiate } = this;
        if (typeof dynamicInstantiate !== 'function') {
          throw new errors.Error('ERR_MISSING_DYNAMIC_INSTANTIATE_HOOK');
        }

        loaderInstance = async (url) => {
          const { exports, execute } = await dynamicInstantiate(url);
          return createDynamicModule(exports, url, (reflect) => {
            debug(`Loading custom loader ${url}`);
            execute(reflect.exports);
          });
        };
      } else {
        loaderInstance = ModuleRequest.loaders.get(format);
      }
      job = new ModuleJob(this, url, loaderInstance);
      this.moduleMap.set(url, job);
    }
    return job;
  }

  async import(specifier, parentURL = this.base) {
    const job = await this.getModuleJob(specifier, parentURL);
    const module = await job.run();
    return module.namespace();
  }
}
Loader.validFormats = ['esm', 'cjs', 'builtin', 'addon', 'json', 'dynamic'];
Object.setPrototypeOf(Loader.prototype, null);
module.exports = Loader;
