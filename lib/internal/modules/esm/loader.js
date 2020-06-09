'use strict';

// This is needed to avoid cycles in esm/resolve <-> cjs/loader
require('internal/modules/cjs/loader');

const {
  FunctionPrototypeBind,
  ObjectSetPrototypeOf,
} = primordials;

const {
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_RETURN_PROPERTY,
  ERR_INVALID_RETURN_PROPERTY_VALUE,
  ERR_INVALID_RETURN_VALUE,
  ERR_UNKNOWN_MODULE_FORMAT,
} = require('internal/errors').codes;
const { URL, pathToFileURL } = require('internal/url');
const { validateString } = require('internal/validators');
const ModuleMap = require('internal/modules/esm/module_map');
const ModuleJob = require('internal/modules/esm/module_job');

const {
  defaultResolve,
  DEFAULT_CONDITIONS,
} = require('internal/modules/esm/resolve');
const { defaultGetFormat } = require('internal/modules/esm/get_format');
const { defaultGetSource } = require(
  'internal/modules/esm/get_source');
const { translators } = require(
  'internal/modules/esm/translators');
const { getOptionValue } = require('internal/options');
const {
  isArrayBufferView,
  isAnyArrayBuffer,
} = require('internal/util/types');

let cwd; // Initialized in importLoader

function validateSource(source, hookName, allowString) {
  if (allowString && typeof source === 'string') {
    return;
  }
  if (isArrayBufferView(source) || isAnyArrayBuffer(source)) {
    return;
  }
  throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
    `${allowString ? 'string, ' : ''}array buffer, or typed array`,
    hookName,
    'source',
    source,
  );
}

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

    // Preload code is provided by loaders to be run after hook initialization.
    this.globalPreloadCode = [];
    // Loader resolve hook.
    this._resolve = defaultResolve;
    // Loader getFormat hook.
    this._getFormat = defaultGetFormat;
    // Loader getSource hook.
    this._getSource = defaultGetSource;
    // Transform source hooks.
    this.transformSourceHooks = [];
    // The index for assigning unique URLs to anonymous module evaluation
    this.evalIndex = 0;
  }

  async resolve(specifier, parentURL) {
    const isMain = parentURL === undefined;
    if (!isMain)
      validateString(parentURL, 'parentURL');

    const resolveResponse = await this._resolve(
      specifier, { parentURL, conditions: DEFAULT_CONDITIONS });
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
    const getFormatResponse = await this._getFormat(url, {});
    if (typeof getFormatResponse !== 'object') {
      throw new ERR_INVALID_RETURN_VALUE(
        'object', 'loader getFormat', getFormatResponse);
    }

    const { format } = getFormatResponse;
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
      !url.startsWith('file:') &&
      !url.startsWith('data:')
    ) {
      throw new ERR_INVALID_RETURN_PROPERTY(
        'file: or data: url', 'loader resolve', 'url', url
      );
    }

    return format;
  }

  async getSource(url, format) {
    const { source: originalSource } = await this._getSource(url, { format });

    const allowString = format !== 'wasm';
    validateSource(originalSource, 'getSource', allowString);

    let source = originalSource;
    for (let i = 0; i < this.transformSourceHooks.length; i += 1) {
      const hook = this.transformSourceHooks[i];
      ({ source } = await hook(source, { url, format, originalSource }));
      validateSource(source, 'transformSource', allowString);
    }

    return source;
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

  async importLoader(specifier) {
    if (cwd === undefined) {
      try {
        // `process.cwd()` can fail.
        cwd = process.cwd() + '/';
      } catch {
        cwd = 'file:///';
      }
      cwd = pathToFileURL(cwd).href;
    }

    const { url } = await defaultResolve(specifier, cwd,
                                         { conditions: DEFAULT_CONDITIONS });
    const { format } = await defaultGetFormat(url, {});

    // !!! CRITICAL SECTION !!!
    // NO AWAIT OPS BETWEEN HERE AND SETTING JOB IN MODULE MAP!
    // YIELDING CONTROL COULD RESULT IN MAP BEING OVERRIDDEN!
    let job = this.moduleMap.get(url);
    if (job === undefined) {
      if (!translators.has(format))
        throw new ERR_UNKNOWN_MODULE_FORMAT(format);

      const loaderInstance = translators.get(format);

      job = new ModuleJob(this, url, loaderInstance, false, false);
      this.moduleMap.set(url, job);
      // !!! END CRITICAL SECTION !!!
    }

    const { module } = await job.run();
    return module.getNamespace();
  }

  hook(hooks) {
    const {
      resolve,
      getFormat,
      getSource,
    } = hooks;

    // Use .bind() to avoid giving access to the Loader instance when called.
    if (resolve !== undefined)
      this._resolve = FunctionPrototypeBind(resolve, null);
    if (getFormat !== undefined) {
      this._getFormat = FunctionPrototypeBind(getFormat, null);
    }
    if (getSource !== undefined) {
      this._getSource = FunctionPrototypeBind(getSource, null);
    }
  }

  runGlobalPreloadCode() {
    for (let i = 0; i < this.globalPreloadCode.length; i += 1) {
      const preloadCode = this.globalPreloadCode[i];

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
  }

  async getModuleJob(specifier, parentURL) {
    const url = await this.resolve(specifier, parentURL);
    const format = await this.getFormat(url);

    // !!! CRITICAL SECTION !!!
    // NO AWAIT OPS BETWEEN HERE AND SETTING JOB IN MODULE MAP
    let job = this.moduleMap.get(url);
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
    // !!! END CRITICAL SECTION !!!

    return job;
  }
}

ObjectSetPrototypeOf(Loader.prototype, null);

exports.Loader = Loader;
