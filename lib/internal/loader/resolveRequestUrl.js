'use strict';

const { URL } = require('url');
const internalCJSModule = require('internal/module');
const internalURLModule = require('internal/url');
const internalFS = require('internal/fs');
const NativeModule = require('native_module');
const { extname } = require('path');
const { realpathSync } = require('fs');
const preserveSymlinks = !!process.binding('config').preserveSymlinks;
const {
  ModuleWrap,
  createDynamicModule
} = require('internal/loader/ModuleWrap');
const errors = require('internal/errors');

const search = require('internal/loader/search');
const asyncReadFile = require('util').promisify(require('fs').readFile);
const debug = require('util').debuglog('esm');

const realpathCache = new Map();

class ModuleRequest {
  constructor(url) {
    this.url = url;
  }
}
Object.setPrototypeOf(ModuleRequest.prototype, null);

// Strategy for loading a standard JavaScript module
class StandardModuleRequest extends ModuleRequest {
  async createModule() {
    const source = `${await asyncReadFile(this.url)}`;
    debug(`Loading StandardModule ${this.url}`);
    return new ModuleWrap(internalCJSModule.stripShebang(source),
                          `${this.url}`);
  }
}

// Strategy for loading a node-style CommonJS module
class CJSModuleRequest extends ModuleRequest {
  async createModule() {
    const ctx = createDynamicModule(['default'], this.url, (reflect) => {
      debug(`Loading CJSModule ${this.url.pathname}`);
      const CJSModule = require('module');
      const pathname = internalURLModule.getPathFromURL(this.url);
      CJSModule._load(pathname);
    });
    this.finish = (module) => {
      ctx.reflect.exports.default.set(module.exports);
    };
    return ctx.module;
  }
}

// Strategy for loading a node builtin CommonJS module that isn't
// through normal resolution
class NativeModuleRequest extends CJSModuleRequest {
  async createModule() {
    const ctx = createDynamicModule(['default'], this.url, (reflect) => {
      debug(`Loading NativeModule ${this.url.pathname}`);
      const exports = require(this.url.pathname);
      reflect.exports.default.set(exports);
    });
    return ctx.module;
  }
}

const normalizeBaseURL = (baseURLOrString) => {
  if (baseURLOrString instanceof URL) return baseURLOrString;
  if (typeof baseURLOrString === 'string') return new URL(baseURLOrString);
  return undefined;
};

const resolveRequestUrl = (baseURLOrString, specifier) => {
  if (NativeModule.nonInternalExists(specifier)) {
    return new NativeModuleRequest(new URL(`node:${specifier}`));
  }

  const baseURL = normalizeBaseURL(baseURLOrString);
  let url = search(specifier, baseURL);

  if (url.protocol !== 'file:') {
    throw new errors.Error('ERR_INVALID_PROTOCOL', url.protocol, 'file:');
  }

  if (!preserveSymlinks) {
    const real = realpathSync(internalURLModule.getPathFromURL(url), {
      [internalFS.realpathCacheKey]: realpathCache
    });
    const old = url;
    url = internalURLModule.getURLFromFilePath(real);
    url.search = old.search;
    url.hash = old.hash;
  }

  const ext = extname(url.pathname);
  if (ext === '.mjs') {
    return new StandardModuleRequest(url);
  }

  return new CJSModuleRequest(url);
};
module.exports = resolveRequestUrl;
