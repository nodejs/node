'use strict';

const {
  Array,
  ArrayIsArray,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ArrayPrototypeForEach,
  ArrayPrototypeMap,
  Boolean,
  FunctionPrototypeCall,
  JSONParse,
  ObjectAssign,
  ObjectCreate,
  ObjectDefineProperty,
  ObjectKeys,
  ObjectPrototypeHasOwnProperty,
  ObjectSetPrototypeOf,
  RegExpPrototypeExec,
  SafePromiseAllReturnArrayLike,
  SafeArrayIterator,
  SafeWeakMap,
  SafeMap,
  SafeSet,
  StringPrototypeReplaceAll,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  StringPrototypeToUpperCase,
  globalThis,
  JSONStringify,
  PromisePrototypeThen,
  PromiseReject,
} = primordials;

const { WebAssembly } = globalThis;

const {
  ERR_LOADER_CHAIN_INCOMPLETE,
  ERR_INTERNAL_ASSERTION,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_RETURN_PROPERTY_VALUE,
  ERR_INVALID_RETURN_VALUE,
  ERR_UNKNOWN_MODULE_FORMAT,
  ERR_UNKNOWN_BUILTIN_MODULE,
} = require('internal/errors').codes;

const {
  pathToFileURL, isURLInstance, URL, fileURLToPath,
} = require('internal/url');
const { emitExperimentalWarning, getLazy } = require('internal/util');
const { isAnyArrayBuffer, isArrayBufferView } = require('internal/util/types');
const { readFileSync } = require('fs');
const { extname, isAbsolute } = require('path');
const { validateObject, validateString } = require('internal/validators');
const { getOptionValue } = require('internal/options');
const getCascadedLoader = getLazy(
  () => require('internal/modules/esm/cascaded_loader').getCascadedLoader()
);
const {
  defaultResolve,
} = require('internal/modules/esm/resolve');

const {
  loadBuiltinModule,
  stripBOM,
  enrichCJSError,
} = require('internal/modules/helpers');
const {
  Module: CJSModule,
  cjsParseCache
} = require('internal/modules/cjs/loader');

let debug = require('internal/util/debuglog').debuglog('esm', (fn) => {
  debug = fn;
});

const { ModuleWrap } = internalBinding('module_wrap');
const {
  setCallbackForWrap,
  ModuleMap,
  ModuleJob,
  getDefaultConditions,
} = require('internal/modules/esm/utils');

const isWindows = process.platform === 'win32';
let DECODER = null;

let cjsParse;
async function initCJSParse() {
  if (typeof WebAssembly === 'undefined') {
    cjsParse = require('internal/deps/cjs-module-lexer/lexer').parse;
  } else {
    const { parse, init } =
        require('internal/deps/cjs-module-lexer/dist/lexer');
    try {
      await init();
      cjsParse = parse;
    } catch {
      cjsParse = require('internal/deps/cjs-module-lexer/lexer').parse;
    }
  }
}

function createImport(impt, index) {
  const imptPath = JSONStringify(impt);
  return `import * as $import_${index} from ${imptPath};
import.meta.imports[${imptPath}] = $import_${index};`;
}

function createExport(expt) {
  const name = `${expt}`;
  return `let $${name};
export { $${name} as ${name} };
import.meta.exports.${name} = {
  get: () => $${name},
  set: (v) => $${name} = v,
};`;
}

function createDynamicModule(imports, exports, url = '', evaluate) {
  debug('creating ESM facade for %s with exports: %j', url, exports);
  const source = `
${ArrayPrototypeJoin(ArrayPrototypeMap(imports, createImport), '\n')}
${ArrayPrototypeJoin(ArrayPrototypeMap(exports, createExport), '\n')}
import.meta.done();
`;
  const m = new ModuleWrap(`${url}`, undefined, source, 0, 0);

  const readyfns = new SafeSet();
  const reflect = {
    exports: ObjectCreate(null),
    onReady: (cb) => { readyfns.add(cb); },
  };

  if (imports.length)
    reflect.imports = ObjectCreate(null);

  setCallbackForWrap(m, {
    initializeImportMeta: (meta, wrap) => {
      meta.exports = reflect.exports;
      if (reflect.imports)
        meta.imports = reflect.imports;
      meta.done = () => {
        evaluate(reflect);
        reflect.onReady = (cb) => cb(reflect);
        for (const fn of readyfns) {
          readyfns.delete(fn);
          fn(reflect);
        }
      };
    },
  });

  return {
    module: m,
    reflect,
  };
}

function assertBufferSource(body, allowString, hookName) {
  if (allowString && typeof body === 'string') {
    return;
  }
  if (isArrayBufferView(body) || isAnyArrayBuffer(body)) {
    return;
  }
  throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
    `${allowString ? 'string, ' : ''}array buffer, or typed array`,
    hookName,
    'source',
    body
  );
}

function stringify(body) {
  if (typeof body === 'string') return body;
  assertBufferSource(body, false, 'transformSource');
  const { TextDecoder } = require('internal/encoding');
  DECODER = DECODER === null ? new TextDecoder() : DECODER;
  return DECODER.decode(body);
}

function errPath(url) {
  const parsed = new URL(url);
  if (parsed.protocol === 'file:') {
    return fileURLToPath(parsed);
  }
  return url;
}

async function moduleStrategy(url, source, isMain) {
  assertBufferSource(source, true, 'load');
  source = stringify(source);
  const { maybeCacheSourceMap } = require('internal/source_map/source_map_cache');
  maybeCacheSourceMap(url, source);
  debug(`Translating StandardModule ${url}`);
  const module = new ModuleWrap(url, undefined, source, 0, 0);
  setCallbackForWrap(module, {
    initializeImportMeta: (meta, wrap) => this.importMetaInitialize(meta, { url }),
    importModuleDynamically: async function(specifier, { url }, assertions) {
      const cascadedLoader = getCascadedLoader();
      return cascadedLoader.import(specifier, url, assertions);
    },
  });
  return module;
}

async function commonjsStrategy(url, source, isMain) {
  debug(`Translating CJSModule ${url}`);

  let filename = fileURLToPath(new URL(url));
  if (isWindows)
    filename = StringPrototypeReplaceAll(filename, '/', '\\');

  if (!cjsParse) await initCJSParse();
  const { module, exportNames } = cjsPreparseModuleExports(filename);
  const namesWithDefault = exportNames.has('default') ?
    [...exportNames] : ['default', ...exportNames];

  return new ModuleWrap(url, undefined, namesWithDefault, function() {
    debug(`Loading CJSModule ${url}`);

    let exports;
    const cascadedLoader = getCascadedLoader();
    if (cascadedLoader.cjsCache.has(module)) {
      exports = cascadedLoader.cjsCache.get(module);
      cascadedLoader.cjsCache.delete(module);
    } else {
      try {
        exports = CJSModule._load(filename, undefined, isMain);
      } catch (err) {
        enrichCJSError(err, undefined, filename);
        throw err;
      }
    }

    for (const exportName of exportNames) {
      if (!ObjectPrototypeHasOwnProperty(exports, exportName) ||
          exportName === 'default')
        continue;
      // We might trigger a getter -> dont fail.
      let value;
      try {
        value = exports[exportName];
      } catch {
        // Continue regardless of error.
      }
      this.setExport(exportName, value);
    }
    this.setExport('default', exports);
  });
}

function cjsPreparseModuleExports(filename) {
  let module = CJSModule._cache[filename];
  if (module) {
    const cached = cjsParseCache.get(module);
    if (cached)
      return { module, exportNames: cached.exportNames };
  }
  const loaded = Boolean(module);
  if (!loaded) {
    module = new CJSModule(filename);
    module.filename = filename;
    module.paths = CJSModule._nodeModulePaths(module.path);
    CJSModule._cache[filename] = module;
  }

  let source;
  try {
    source = readFileSync(filename, 'utf8');
  } catch {
    // Continue regardless of error.
  }

  let exports, reexports;
  try {
    ({ exports, reexports } = cjsParse(source || ''));
  } catch {
    exports = [];
    reexports = [];
  }

  const exportNames = new SafeSet(new SafeArrayIterator(exports));

  // Set first for cycles.
  cjsParseCache.set(module, { source, exportNames, loaded });

  if (reexports.length) {
    module.filename = filename;
    module.paths = CJSModule._nodeModulePaths(module.path);
  }
  ArrayPrototypeForEach(reexports, (reexport) => {
    let resolved;
    try {
      resolved = CJSModule._resolveFilename(reexport, module);
    } catch {
      return;
    }
    const ext = extname(resolved);
    if ((ext === '.js' || ext === '.cjs' || !CJSModule._extensions[ext]) &&
        isAbsolute(resolved)) {
      const { exportNames: reexportNames } = cjsPreparseModuleExports(resolved);
      for (const name of reexportNames)
        exportNames.add(name);
    }
  });

  return { module, exportNames };
}

async function builtinStrategy(url) {
  debug(`Translating BuiltinModule ${url}`);
  // Slice 'node:' scheme
  const id = StringPrototypeSlice(url, 5);
  const module = loadBuiltinModule(id, url);
  if (!StringPrototypeStartsWith(url, 'node:') || !module) {
    throw new ERR_UNKNOWN_BUILTIN_MODULE(url);
  }
  debug(`Loading BuiltinModule ${url}`);
  return module.getESMFacade();
}

async function jsonStrategy(url, source) {
  emitExperimentalWarning('Importing JSON modules');
  assertBufferSource(source, true, 'load');
  debug(`Loading JSONModule ${url}`);
  const pathname = StringPrototypeStartsWith(url, 'file:') ?
    fileURLToPath(url) : null;
  let modulePath;
  let module;
  if (pathname) {
    modulePath = isWindows ?
      StringPrototypeReplaceAll(pathname, '/', '\\') : pathname;
    module = CJSModule._cache[modulePath];
    if (module && module.loaded) {
      const exports = module.exports;
      return new ModuleWrap(url, undefined, ['default'], function() {
        this.setExport('default', exports);
      });
    }
  }
  source = stringify(source);
  if (pathname) {
    // A require call could have been called on the same file during loading and
    // that resolves synchronously. To make sure we always return the identical
    // export, we have to check again if the module already exists or not.
    module = CJSModule._cache[modulePath];
    if (module && module.loaded) {
      const exports = module.exports;
      return new ModuleWrap(url, undefined, ['default'], function() {
        this.setExport('default', exports);
      });
    }
  }
  try {
    const exports = JSONParse(stripBOM(source));
    module = {
      exports,
      loaded: true
    };
  } catch (err) {
    // TODO (BridgeAR): We could add a NodeCore error that wraps the JSON
    // parse error instead of just manipulating the original error message.
    // That would allow to add further properties and maybe additional
    // debugging information.
    err.message = errPath(url) + ': ' + err.message;
    throw err;
  }
  if (pathname) {
    CJSModule._cache[modulePath] = module;
  }
  return new ModuleWrap(url, undefined, ['default'], function() {
    debug(`Parsing JSONModule ${url}`);
    this.setExport('default', module.exports);
  });
}

async function wasmStrategy(url, source) {
  emitExperimentalWarning('Importing WebAssembly modules');

  assertBufferSource(source, false, 'load');

  debug(`Translating WASMModule ${url}`);

  let compiled;
  try {
    compiled = await WebAssembly.compile(source);
  } catch (err) {
    err.message = errPath(url) + ': ' + err.message;
    throw err;
  }

  const imports =
      ArrayPrototypeMap(WebAssembly.Module.imports(compiled),
                        ({ module }) => module);
  const exports =
    ArrayPrototypeMap(WebAssembly.Module.exports(compiled),
                      ({ name }) => name);

  return createDynamicModule(imports, exports, url, (reflect) => {
    const { exports } = new WebAssembly.Instance(compiled, reflect.imports);
    for (const expt of ObjectKeys(exports))
      reflect.exports[expt].set(exports[expt]);
  }).module;
}

const translators = new SafeMap([
  // Strategy for loading a standard JavaScript module.
  ['module', moduleStrategy],
  // Strategy for loading a node-style CommonJS module
  ['commonjs', commonjsStrategy],
  // Strategy for loading a node builtin CommonJS module that isn't
  // through normal resolution
  ['builtin', builtinStrategy],
  // Strategy for loading a JSON file
  ['json', jsonStrategy],
  // Strategy for loading a wasm module
  ['wasm', wasmStrategy],
]);

/**
 * @typedef {object} ExportedHooks
 * @property {Function} globalPreload Global preload hook.
 * @property {Function} resolve Resolve hook.
 * @property {Function} load Load hook.
 */

/**
 * @typedef {Record<string, any>} ModuleExports
 */

/**
 * @typedef {object} KeyedExports
 * @property {ModuleExports} exports The contents of the module.
 * @property {URL['href']} url The URL of the module.
 */

/**
 * @typedef {object} KeyedHook
 * @property {Function} fn The hook function.
 * @property {URL['href']} url The URL of the module.
 */

/**
 * @typedef {'builtin'|'commonjs'|'json'|'module'|'wasm'} ModuleFormat
 */

/**
 * @typedef {ArrayBuffer|TypedArray|string} ModuleSource
 */

// [2] `validate...()`s throw the wrong error

/**
 * A utility function to iterate through a hook chain, track advancement in the
 * chain, and generate and supply the `next<HookName>` argument to the custom
 * hook.
 * @param {KeyedHook[]} chain The whole hook chain.
 * @param {object} meta Properties that change as the current hook advances
 * along the chain.
 * @param {boolean} meta.chainFinished Whether the end of the chain has been
 * reached AND invoked.
 * @param {string} meta.hookErrIdentifier A user-facing identifier to help
 *  pinpoint where an error occurred. Ex "file:///foo.mjs 'resolve'".
 * @param {number} meta.hookIndex A non-negative integer tracking the current
 * position in the hook chain.
 * @param {string} meta.hookName The kind of hook the chain is (ex 'resolve')
 * @param {boolean} meta.shortCircuited Whether a hook signaled a short-circuit.
 * @param {(hookErrIdentifier, hookArgs) => void} validate A wrapper function
 *  containing all validation of a custom loader hook's intermediary output. Any
 *  validation within MUST throw.
 * @returns {function next<HookName>(...hookArgs)} The next hook in the chain.
 */
function nextHookFactory(chain, meta, { validateArgs, validateOutput }) {
  // First, prepare the current
  const { hookName } = meta;
  const {
    fn: hook,
    url: hookFilePath,
  } = chain[meta.hookIndex];

  // ex 'nextResolve'
  const nextHookName = `next${
    StringPrototypeToUpperCase(hookName[0]) +
    StringPrototypeSlice(hookName, 1)
  }`;

  // When hookIndex is 0, it's reached the default, which does not call next()
  // so feed it a noop that blows up if called, so the problem is obvious.
  const generatedHookIndex = meta.hookIndex;
  let nextNextHook;
  if (meta.hookIndex > 0) {
    // Now, prepare the next: decrement the pointer so the next call to the
    // factory generates the next link in the chain.
    meta.hookIndex--;

    nextNextHook = nextHookFactory(chain, meta, { validateArgs, validateOutput });
  } else {
    // eslint-disable-next-line func-name-matching
    nextNextHook = function chainAdvancedTooFar() {
      throw new ERR_INTERNAL_ASSERTION(
        `ESM custom loader '${hookName}' advanced beyond the end of the chain.`
      );
    };
  }

  return ObjectDefineProperty(
    async (arg0 = undefined, context) => {
      // Update only when hook is invoked to avoid fingering the wrong filePath
      meta.hookErrIdentifier = `${hookFilePath} '${hookName}'`;

      validateArgs(`${meta.hookErrIdentifier} hook's ${nextHookName}()`, arg0, context);

      const outputErrIdentifier = `${chain[generatedHookIndex].url} '${hookName}' hook's ${nextHookName}()`;

      // Set when next<HookName> is actually called, not just generated.
      if (generatedHookIndex === 0) { meta.chainFinished = true; }

      if (context) { // `context` has already been validated, so no fancy check needed.
        ObjectAssign(meta.context, context);
      }

      const output = await hook(arg0, meta.context, nextNextHook);

      validateOutput(outputErrIdentifier, output);

      if (output?.shortCircuit === true) { meta.shortCircuited = true; }
      return output;

    },
    'name',
    { __proto__: null, value: nextHookName },
  );
}

function createImportMetaResolve(defaultParentUrl) {
  return async function resolve(specifier, parentUrl = defaultParentUrl) {
    const loader = getCascadedLoader();
    return PromisePrototypeThen(
      loader.resolve(specifier, parentUrl),
      ({ url }) => url,
      (error) => (
        error.code === 'ERR_UNSUPPORTED_DIR_IMPORT' ?
          error.url : PromiseReject(error))
    );
  };
}

/**
 * @param {object} meta
 * @param {{url: string}} context
 */
function defaultInitializeImportMeta(meta, context) {
  const { url } = context;

  // Alphabetical
  if (getOptionValue('--experimental-import-meta-resolve')) {
    meta.resolve = createImportMetaResolve(url);
  }

  meta.url = url;
}

/**
 * An ESMLoader instance is used as the main entry point for loading ES modules.
 * Currently, this is a singleton -- there is only one used for loading
 * the main module and everything in its dependency graph.
 */

class ESMLoader {
  #hooks = {
    /**
     * Prior to ESM loading. These are called once before any modules are started.
     * @private
     * @property {KeyedHook[]} globalPreload Last-in-first-out list of preload hooks.
     */
    globalPreload: [],

    /**
     * Phase 2 of 2 in ESM loading (phase 1 is below).
     * @private
     * @property {KeyedHook[]} load Last-in-first-out collection of loader hooks.
     */
    load: [
      {
        fn: require('internal/modules/esm/load').defaultLoad,
        url: 'node:internal/modules/esm/load',
      },
    ],

    /**
     * Phase 1 of 2 in ESM loading.
     * @private
     * @property {KeyedHook[]} resolve Last-in-first-out collection of resolve hooks.
     */
    resolve: [
      {
        fn: defaultResolve,
        url: 'node:internal/modules/esm/resolve',
      },
    ],
  };

  #importMetaInitializer = defaultInitializeImportMeta;

  /**
   * Map of already-loaded CJS modules to use
   */
  cjsCache = new SafeWeakMap();

  /**
   * The index for assigning unique URLs to anonymous module evaluation
   */
  evalIndex = 0;

  /**
   * Registry of loaded modules, akin to `require.cache`
   */
  moduleMap = new ModuleMap();

  /**
   * Methods which translate input code or other information into ES modules
   */
  translators = translators;

  constructor() {
    if (getOptionValue('--experimental-loader').length > 0) {
      emitExperimentalWarning('Custom ESM Loaders');
    }
    if (getOptionValue('--experimental-network-imports')) {
      emitExperimentalWarning('Network Imports');
    }
  }

  /**
   *
   * @param {ModuleExports} exports
   * @returns {ExportedHooks}
   */
  static pluckHooks({
    globalPreload,
    resolve,
    load,
    // obsolete hooks:
    dynamicInstantiate,
    getFormat,
    getGlobalPreloadCode,
    getSource,
    transformSource,
  }) {
    const obsoleteHooks = [];
    const acceptedHooks = ObjectCreate(null);

    if (getGlobalPreloadCode) {
      globalPreload ??= getGlobalPreloadCode;

      process.emitWarning(
        'Loader hook "getGlobalPreloadCode" has been renamed to "globalPreload"'
      );
    }
    if (dynamicInstantiate) ArrayPrototypePush(
      obsoleteHooks,
      'dynamicInstantiate'
    );
    if (getFormat) ArrayPrototypePush(
      obsoleteHooks,
      'getFormat',
    );
    if (getSource) ArrayPrototypePush(
      obsoleteHooks,
      'getSource',
    );
    if (transformSource) ArrayPrototypePush(
      obsoleteHooks,
      'transformSource',
    );

    if (obsoleteHooks.length) process.emitWarning(
      `Obsolete loader hook(s) supplied and will be ignored: ${
        ArrayPrototypeJoin(obsoleteHooks, ', ')
      }`,
      'DeprecationWarning',
    );

    if (globalPreload) {
      acceptedHooks.globalPreload = globalPreload;
    }
    if (resolve) {
      acceptedHooks.resolve = resolve;
    }
    if (load) {
      acceptedHooks.load = load;
    }

    return acceptedHooks;
  }

  /**
   * Collect custom/user-defined hook(s). After all hooks have been collected,
   * calls global preload hook(s).
   * @param {KeyedExports} customLoaders
   *  A list of exports from user-defined loaders (as returned by
   *  ESMLoader.import()).
   */
  addCustomLoaders(
    customLoaders = [],
  ) {
    for (let i = 0; i < customLoaders.length; i++) {
      const {
        exports,
        url,
      } = customLoaders[i];
      const {
        globalPreload,
        resolve,
        load,
      } = ESMLoader.pluckHooks(exports);

      if (globalPreload) {
        ArrayPrototypePush(
          this.#hooks.globalPreload,
          {
            fn: globalPreload,
            url,
          },
        );
      }
      if (resolve) {
        ArrayPrototypePush(
          this.#hooks.resolve,
          {
            fn: resolve,
            url,
          },
        );
      }
      if (load) {
        ArrayPrototypePush(
          this.#hooks.load,
          {
            fn: load,
            url,
          },
        );
      }
    }

    this.preload();
  }

  async eval(
    source,
    url = pathToFileURL(`${process.cwd()}/[eval${++this.evalIndex}]`).href
  ) {
    const evalInstance = (url) => {
      const module = new ModuleWrap(url, undefined, source, 0, 0);
      setCallbackForWrap(module, {
        importModuleDynamically: (specifier, { url }, importAssertions) => {
          return this.import(specifier, url, importAssertions);
        }
      });

      return module;
    };
    const job = new ModuleJob(
      this, url, undefined, evalInstance, false, false);
    this.moduleMap.set(url, undefined, job);
    const { module } = await job.run();

    return {
      __proto__: null,
      namespace: module.getNamespace(),
    };
  }

  /**
   * Get a (possibly still pending) module job from the cache,
   * or create one and return its Promise.
   * @param {string} specifier The string after `from` in an `import` statement,
   *                           or the first parameter of an `import()`
   *                           expression
   * @param {string | undefined} parentURL The URL of the module importing this
   *                                     one, unless this is the Node.js entry
   *                                     point.
   * @param {Record<string, string>} importAssertions Validations for the
   *                                                  module import.
   * @returns {Promise<ModuleJob>} The (possibly pending) module job
   */
  async getModuleJob(specifier, parentURL, importAssertions) {
    let importAssertionsForResolve;

    // By default, `this.#hooks.load` contains just the Node default load hook
    if (this.#hooks.load.length !== 1) {
      // We can skip cloning if there are no user-provided loaders because
      // the Node.js default resolve hook does not use import assertions.
      importAssertionsForResolve = {
        __proto__: null,
        ...importAssertions,
      };
    }

    const { format, url } =
      await this.resolve(specifier, parentURL, importAssertionsForResolve);

    let job = this.moduleMap.get(url, importAssertions.type);

    // CommonJS will set functions for lazy job evaluation.
    if (typeof job === 'function') {
      this.moduleMap.set(url, undefined, job = job());
    }

    if (job === undefined) {
      job = this.#createModuleJob(url, importAssertions, parentURL, format);
    }

    return job;
  }

  /**
   * Create and cache an object representing a loaded module.
   * @param {string} url The absolute URL that was resolved for this module
   * @param {Record<string, string>} importAssertions Validations for the
   *                                                  module import.
   * @param {string} [parentURL] The absolute URL of the module importing this
   *                             one, unless this is the Node.js entry point
   * @param {string} [format] The format hint possibly returned by the
   *                          `resolve` hook
   * @returns {Promise<ModuleJob>} The (possibly pending) module job
   */
  #createModuleJob(url, importAssertions, parentURL, format) {
    const moduleProvider = async (url, isMain) => {
      const {
        format: finalFormat,
        responseURL,
        source,
      } = await this.load(url, {
        format,
        importAssertions,
      });

      const translator = translators.get(finalFormat);

      if (!translator) {
        throw new ERR_UNKNOWN_MODULE_FORMAT(finalFormat, responseURL);
      }

      return FunctionPrototypeCall(translator, this, responseURL, source, isMain);
    };

    const inspectBrk = (
      parentURL === undefined &&
      getOptionValue('--inspect-brk')
    );

    if (process.env.WATCH_REPORT_DEPENDENCIES && process.send) {
      process.send({ 'watch:import': [url] });
    }
    const job = new ModuleJob(
      this,
      url,
      importAssertions,
      moduleProvider,
      parentURL === undefined,
      inspectBrk
    );

    this.moduleMap.set(url, importAssertions.type, job);

    return job;
  }

  /**
   * This method is usually called indirectly as part of the loading processes.
   * Internally, it is used directly to add loaders. Use directly with caution.
   *
   * This method must NOT be renamed: it functions as a dynamic import on a
   * loader module.
   *
   * @param {string | string[]} specifiers Path(s) to the module.
   * @param {string} parentURL Path of the parent importing the module.
   * @param {Record<string, string>} importAssertions Validations for the
   *                                                  module import.
   * @returns {Promise<ExportedHooks | KeyedExports[]>}
   *  A collection of module export(s) or a list of collections of module
   *  export(s).
   */
  async import(specifiers, parentURL, importAssertions) {
    // For loaders, `import` is passed multiple things to process, it returns a
    // list pairing the url and exports collected. This is especially useful for
    // error messaging, to identity from where an export came. But, in most
    // cases, only a single url is being "imported" (ex `import()`), so there is
    // only 1 possible url from which the exports were collected and it is
    // already known to the caller. Nesting that in a list would only ever
    // create redundant work for the caller, so it is later popped off the
    // internal list.
    const wasArr = ArrayIsArray(specifiers);
    if (!wasArr) { specifiers = [specifiers]; }

    const count = specifiers.length;
    const jobs = new Array(count);

    for (let i = 0; i < count; i++) {
      jobs[i] = this.getModuleJob(specifiers[i], parentURL, importAssertions)
        .then((job) => job.run())
        .then(({ module }) => module.getNamespace());
    }

    const namespaces = await SafePromiseAllReturnArrayLike(jobs);

    if (!wasArr) { return namespaces[0]; } // We can skip the pairing below

    for (let i = 0; i < count; i++) {
      namespaces[i] = {
        __proto__: null,
        url: specifiers[i],
        exports: namespaces[i],
      };
    }

    return namespaces;
  }

  /**
   * Provide source that is understood by one of Node's translators.
   *
   * Internally, this behaves like a backwards iterator, wherein the stack of
   * hooks starts at the top and each call to `nextLoad()` moves down 1 step
   * until it reaches the bottom or short-circuits.
   *
   * @param {URL['href']} url The URL/path of the module to be loaded
   * @param {object} context Metadata about the module
   * @returns {{ format: ModuleFormat, source: ModuleSource }}
   */
  async load(url, context = {}) {
    const chain = this.#hooks.load;
    const meta = {
      chainFinished: null,
      context,
      hookErrIdentifier: '',
      hookIndex: chain.length - 1,
      hookName: 'load',
      shortCircuited: false,
    };

    const validateArgs = (hookErrIdentifier, nextUrl, ctx) => {
      if (typeof nextUrl !== 'string') {
        // non-strings can be coerced to a url string
        // validateString() throws a less-specific error
        throw new ERR_INVALID_ARG_TYPE(
          `${hookErrIdentifier} url`,
          'a url string',
          nextUrl,
        );
      }

      // Try to avoid expensive URL instantiation for known-good urls
      if (!this.moduleMap.has(nextUrl)) {
        try {
          new URL(nextUrl);
        } catch {
          throw new ERR_INVALID_ARG_VALUE(
            `${hookErrIdentifier} url`,
            nextUrl,
            'should be a url string',
          );
        }
      }

      if (ctx) validateObject(ctx, `${hookErrIdentifier} context`);
    };
    const validateOutput = (hookErrIdentifier, output) => {
      if (typeof output !== 'object' || output === null) { // [2]
        throw new ERR_INVALID_RETURN_VALUE(
          'an object',
          hookErrIdentifier,
          output,
        );
      }
    };

    const nextLoad = nextHookFactory(chain, meta, { validateArgs, validateOutput });

    const loaded = await nextLoad(url, context);
    const { hookErrIdentifier } = meta; // Retrieve the value after all settled

    validateOutput(hookErrIdentifier, loaded);

    if (loaded?.shortCircuit === true) { meta.shortCircuited = true; }

    if (!meta.chainFinished && !meta.shortCircuited) {
      throw new ERR_LOADER_CHAIN_INCOMPLETE(hookErrIdentifier);
    }

    const {
      format,
      source,
    } = loaded;
    let responseURL = loaded.responseURL;

    if (responseURL === undefined) {
      responseURL = url;
    }

    let responseURLObj;
    if (typeof responseURL === 'string') {
      try {
        responseURLObj = new URL(responseURL);
      } catch {
        // responseURLObj not defined will throw in next branch.
      }
    }

    if (responseURLObj?.href !== responseURL) {
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
        'undefined or a fully resolved URL string',
        hookErrIdentifier,
        'responseURL',
        responseURL,
      );
    }

    if (format == null) {
      const dataUrl = RegExpPrototypeExec(
        /^data:([^/]+\/[^;,]+)(?:[^,]*?)(;base64)?,/,
        url,
      );

      throw new ERR_UNKNOWN_MODULE_FORMAT(
        dataUrl ? dataUrl[1] : format,
        url);
    }

    if (typeof format !== 'string') { // [2]
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
        'a string',
        hookErrIdentifier,
        'format',
        format,
      );
    }

    if (
      source != null &&
      typeof source !== 'string' &&
      !isAnyArrayBuffer(source) &&
      !isArrayBufferView(source)
    ) {
      throw ERR_INVALID_RETURN_PROPERTY_VALUE(
        'a string, an ArrayBuffer, or a TypedArray',
        hookErrIdentifier,
        'source',
        source
      );
    }

    return {
      __proto__: null,
      format,
      responseURL,
      source,
    };
  }

  preload() {
    for (let i = this.#hooks.globalPreload.length - 1; i >= 0; i--) {
      const { MessageChannel } = require('internal/worker/io');
      const channel = new MessageChannel();
      const {
        port1: insidePreload,
        port2: insideLoader,
      } = channel;

      insidePreload.unref();
      insideLoader.unref();

      const {
        fn: preload,
        url: specifier,
      } = this.#hooks.globalPreload[i];

      const preloaded = preload({
        port: insideLoader,
      });

      if (preloaded == null) { return; }

      const hookErrIdentifier = `${specifier} globalPreload`;

      if (typeof preloaded !== 'string') { // [2]
        throw new ERR_INVALID_RETURN_VALUE(
          'a string',
          hookErrIdentifier,
          preload,
        );
      }
      const { compileFunction } = require('vm');
      const preloadInit = compileFunction(
        preloaded,
        ['getBuiltin', 'port', 'setImportMetaCallback'],
        {
          filename: '<preload>',
        }
      );
      const { BuiltinModule } = require('internal/bootstrap/loaders');
      // We only allow replacing the importMetaInitializer during preload,
      // after preload is finished, we disable the ability to replace it
      //
      // This exposes accidentally setting the initializer too late by
      // throwing an error.
      let finished = false;
      let replacedImportMetaInitializer = false;
      let next = this.#importMetaInitializer;
      try {
        // Calls the compiled preload source text gotten from the hook
        // Since the parameters are named we use positional parameters
        // see compileFunction above to cross reference the names
        FunctionPrototypeCall(
          preloadInit,
          globalThis,
          // Param getBuiltin
          (builtinName) => {
            if (BuiltinModule.canBeRequiredByUsers(builtinName) &&
                BuiltinModule.canBeRequiredWithoutScheme(builtinName)) {
              return require(builtinName);
            }
            throw new ERR_INVALID_ARG_VALUE('builtinName', builtinName);
          },
          // Param port
          insidePreload,
          // Param setImportMetaCallback
          (fn) => {
            if (finished || typeof fn !== 'function') {
              throw new ERR_INVALID_ARG_TYPE('fn', fn);
            }
            replacedImportMetaInitializer = true;
            const parent = next;
            next = (meta, context) => {
              return fn(meta, context, parent);
            };
          });
      } finally {
        finished = true;
        if (replacedImportMetaInitializer) {
          this.#importMetaInitializer = next;
        }
      }
    }
  }

  importMetaInitialize(meta, context) {
    this.#importMetaInitializer(meta, context);
  }

  /**
   * Resolve the location of the module.
   *
   * Internally, this behaves like a backwards iterator, wherein the stack of
   * hooks starts at the top and each call to `nextResolve()` moves down 1 step
   * until it reaches the bottom or short-circuits.
   *
   * @param {string} originalSpecifier The specified URL path of the module to
   *                                   be resolved.
   * @param {string} [parentURL] The URL path of the module's parent.
   * @param {ImportAssertions} [importAssertions] Assertions from the import
   *                                              statement or expression.
   * @returns {{ format: string, url: URL['href'] }}
   */
  async resolve(
    originalSpecifier,
    parentURL,
    importAssertions = ObjectCreate(null),
  ) {
    const isMain = parentURL === undefined;

    if (
      !isMain &&
      typeof parentURL !== 'string' &&
      !isURLInstance(parentURL)
    ) {
      throw new ERR_INVALID_ARG_TYPE(
        'parentURL',
        ['string', 'URL'],
        parentURL,
      );
    }
    const chain = this.#hooks.resolve;
    const context = {
      conditions: getDefaultConditions(),
      importAssertions,
      parentURL,
    };
    const meta = {
      chainFinished: null,
      context,
      hookErrIdentifier: '',
      hookIndex: chain.length - 1,
      hookName: 'resolve',
      shortCircuited: false,
    };

    const validateArgs = (hookErrIdentifier, suppliedSpecifier, ctx) => {
      validateString(
        suppliedSpecifier,
        `${hookErrIdentifier} specifier`,
      ); // non-strings can be coerced to a url string

      if (ctx) validateObject(ctx, `${hookErrIdentifier} context`);
    };
    const validateOutput = (hookErrIdentifier, output) => {
      if (typeof output !== 'object' || output === null) { // [2]
        throw new ERR_INVALID_RETURN_VALUE(
          'an object',
          hookErrIdentifier,
          output,
        );
      }
    };

    const nextResolve = nextHookFactory(chain, meta, { validateArgs, validateOutput });

    const resolution = await nextResolve(originalSpecifier, context);
    const { hookErrIdentifier } = meta; // Retrieve the value after all settled

    validateOutput(hookErrIdentifier, resolution);

    if (resolution?.shortCircuit === true) { meta.shortCircuited = true; }

    if (!meta.chainFinished && !meta.shortCircuited) {
      throw new ERR_LOADER_CHAIN_INCOMPLETE(hookErrIdentifier);
    }

    const {
      format,
      url,
    } = resolution;

    if (
      format != null &&
      typeof format !== 'string' // [2]
    ) {
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
        'a string',
        hookErrIdentifier,
        'format',
        format,
      );
    }

    if (typeof url !== 'string') {
      // non-strings can be coerced to a url string
      // validateString() throws a less-specific error
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
        'a url string',
        hookErrIdentifier,
        'url',
        url,
      );
    }

    // Try to avoid expensive URL instantiation for known-good urls
    if (!this.moduleMap.has(url)) {
      try {
        new URL(url);
      } catch {
        throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
          'a url string',
          hookErrIdentifier,
          'url',
          url,
        );
      }
    }

    return {
      __proto__: null,
      format,
      url,
    };
  }
}

ObjectSetPrototypeOf(ESMLoader.prototype, null);

module.exports = {
  ESMLoader,
  createDynamicModule,  // for tests
};
