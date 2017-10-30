'use strict';

const { getURLFromFilePath } = require('internal/url');

const { createDynamicModule } = require('internal/loader/ModuleWrap');

const ModuleMap = require('internal/loader/ModuleMap');
const ModuleJob = require('internal/loader/ModuleJob');
const ModuleRequest = require('internal/loader/ModuleRequest');
const errors = require('internal/errors');
const debug = require('util').debuglog('esm');

function getBase() {
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

class Loader {
  constructor(base = getBase()) {
    this.moduleMap = new ModuleMap();
    if (typeof base !== 'string') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'base', 'string');
    }
    this.base = base;
    this.resolver = ModuleRequest.resolve.bind(null);
    this.dynamicInstantiate = undefined;
  }

  hook({ resolve = ModuleRequest.resolve, dynamicInstantiate }) {
    this.resolver = resolve.bind(null);
    this.dynamicInstantiate = dynamicInstantiate;
  }

  async resolve(specifier, parentURL = this.base) {
    if (typeof parentURL !== 'string') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                                 'parentURL', 'string');
    }

    const { url, format } = await this.resolver(specifier, parentURL,
                                                ModuleRequest.resolve);

    if (typeof format !== 'string') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'format',
                                 ['esm', 'cjs', 'builtin', 'addon', 'json']);
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

  async getModuleJob(specifier, parentURL = this.base) {
    const { url, format } = await this.resolve(specifier, parentURL);
    let job = this.moduleMap.get(url);
    if (job === undefined) {
      let loaderInstance;
      if (format === 'dynamic') {
        loaderInstance = async (url) => {
          const { exports, execute } = await this.dynamicInstantiate(url);
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
Object.setPrototypeOf(Loader.prototype, null);
module.exports = Loader;
