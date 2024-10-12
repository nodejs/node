'use strict';

const {
  ArrayPrototypeForEach,
  ArrayPrototypeMap,
  Boolean,
  JSONParse,
  ObjectGetPrototypeOf,
  ObjectPrototypeHasOwnProperty,
  ObjectKeys,
  SafeArrayIterator,
  SafeMap,
  SafeSet,
  StringPrototypeIncludes,
  StringPrototypeReplaceAll,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  SyntaxErrorPrototype,
  globalThis: { WebAssembly },
} = primordials;

/** @type {import('internal/util/types')} */
let _TYPES = null;
/**
 * Lazily loads and returns the internal/util/types module.
 */
function lazyTypes() {
  if (_TYPES !== null) { return _TYPES; }
  return _TYPES = require('internal/util/types');
}

const { readFileSync } = require('fs');
const { extname, isAbsolute } = require('path');
const {
  hasEsmSyntax,
  loadBuiltinModule,
  stripBOM,
} = require('internal/modules/helpers');
const {
  Module: CJSModule,
  cjsParseCache,
} = require('internal/modules/cjs/loader');
const { fileURLToPath, URL } = require('internal/url');
let debug = require('internal/util/debuglog').debuglog('esm', (fn) => {
  debug = fn;
});
const { emitExperimentalWarning } = require('internal/util');
const {
  ERR_UNKNOWN_BUILTIN_MODULE,
  ERR_INVALID_RETURN_PROPERTY_VALUE,
} = require('internal/errors').codes;
const { maybeCacheSourceMap } = require('internal/source_map/source_map_cache');
const moduleWrap = internalBinding('module_wrap');
const { ModuleWrap } = moduleWrap;
const asyncESM = require('internal/process/esm_loader');
const { emitWarningSync } = require('internal/process/warning');

/** @type {import('deps/cjs-module-lexer/lexer.js').parse} */
let cjsParse;
/**
 * Initializes the CommonJS module lexer parser.
 * If WebAssembly is available, it uses the optimized version from the dist folder.
 * Otherwise, it falls back to the JavaScript version from the lexer folder.
 */
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

const translators = new SafeMap();
exports.translators = translators;
exports.enrichCJSError = enrichCJSError;

let DECODER = null;
/**
 * Asserts that the given body is a buffer source (either a string, array buffer, or typed array).
 * Throws an error if the body is not a buffer source.
 * @param {string | ArrayBufferView | ArrayBuffer} body - The body to check.
 * @param {boolean} allowString - Whether or not to allow a string as a valid buffer source.
 * @param {string} hookName - The name of the hook being called.
 * @throws {ERR_INVALID_RETURN_PROPERTY_VALUE} If the body is not a buffer source.
 */
function assertBufferSource(body, allowString, hookName) {
  if (allowString && typeof body === 'string') {
    return;
  }
  const { isArrayBufferView, isAnyArrayBuffer } = lazyTypes();
  if (isArrayBufferView(body) || isAnyArrayBuffer(body)) {
    return;
  }
  throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
    `${allowString ? 'string, ' : ''}array buffer, or typed array`,
    hookName,
    'source',
    body,
  );
}

/**
 * Converts a buffer or buffer-like object to a string.
 * @param {string | ArrayBuffer | ArrayBufferView} body - The buffer or buffer-like object to convert to a string.
 * @returns {string} The resulting string.
 */
function stringify(body) {
  if (typeof body === 'string') { return body; }
  assertBufferSource(body, false, 'load');
  const { TextDecoder } = require('internal/encoding');
  DECODER = DECODER === null ? new TextDecoder() : DECODER;
  return DECODER.decode(body);
}

/**
 * Converts a URL to a file path if the URL protocol is 'file:'.
 * @param {string} url - The URL to convert.
 */
function errPath(url) {
  const parsed = new URL(url);
  if (parsed.protocol === 'file:') {
    return fileURLToPath(parsed);
  }
  return url;
}

/**
 * Dynamically imports a module using the ESM loader.
 * @param {string} specifier - The module specifier to import.
 * @param {object} options - An object containing options for the import.
 * @param {string} options.url - The URL of the module requesting the import.
 * @param {Record<string, string>} [attributes] - An object containing attributes for the import.
 * @returns {Promise<import('internal/modules/esm/loader.js').ModuleExports>} The imported module.
 */
async function importModuleDynamically(specifier, { url }, attributes) {
  return asyncESM.esmLoader.import(specifier, url, attributes);
}

// Strategy for loading a standard JavaScript module.
translators.set('module', async function moduleStrategy(url, source, isMain) {
  assertBufferSource(source, true, 'load');
  source = stringify(source);
  maybeCacheSourceMap(url, source);
  debug(`Translating StandardModule ${url}`);
  const module = new ModuleWrap(url, undefined, source, 0, 0);
  const { registerModule } = require('internal/modules/esm/utils');
  registerModule(module, {
    __proto__: null,
    initializeImportMeta: (meta, wrap) => this.importMetaInitialize(meta, { url }),
    importModuleDynamically,
  });
  return module;
});

/**
 * Provide a more informative error for CommonJS imports.
 * @param {Error | any} err
 * @param {string} [content] Content of the file, if known.
 * @param {string} [filename] Useful only if `content` is unknown.
 */
function enrichCJSError(err, content, filename) {
  if (err != null && ObjectGetPrototypeOf(err) === SyntaxErrorPrototype &&
      hasEsmSyntax(content || readFileSync(filename, 'utf-8'))) {
    // Emit the warning synchronously because we are in the middle of handling
    // a SyntaxError that will throw and likely terminate the process before an
    // asynchronous warning would be emitted.
    emitWarningSync(
      'To load an ES module, set "type": "module" in the package.json or use ' +
      'the .mjs extension.',
    );
  }
}

// Strategy for loading a node-style CommonJS module
const isWindows = process.platform === 'win32';
translators.set('commonjs', async function commonjsStrategy(url, source,
                                                            isMain) {
  debug(`Translating CJSModule ${url}`);

  const filename = fileURLToPath(new URL(url));

  if (!cjsParse) { await initCJSParse(); }
  const { module, exportNames } = cjsPreparseModuleExports(filename);
  const namesWithDefault = exportNames.has('default') ?
    [...exportNames] : ['default', ...exportNames];

  return new ModuleWrap(url, undefined, namesWithDefault, function() {
    debug(`Loading CJSModule ${url}`);

    let exports;
    if (asyncESM.esmLoader.cjsCache.has(module)) {
      exports = asyncESM.esmLoader.cjsCache.get(module);
      asyncESM.esmLoader.cjsCache.delete(module);
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
          exportName === 'default') {
        continue;
      }
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
});

/**
 * Pre-parses a CommonJS module's exports and re-exports.
 * @param {string} filename - The filename of the module.
 */
function cjsPreparseModuleExports(filename) {
  let module = CJSModule._cache[filename];
  if (module) {
    const cached = cjsParseCache.get(module);
    if (cached) {
      return { module, exportNames: cached.exportNames };
    }
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
      for (const name of reexportNames) {
        exportNames.add(name);
      }
    }
  });

  return { module, exportNames };
}

// Strategy for loading a node builtin CommonJS module that isn't
// through normal resolution
translators.set('builtin', async function builtinStrategy(url) {
  debug(`Translating BuiltinModule ${url}`);
  // Slice 'node:' scheme
  const id = StringPrototypeSlice(url, 5);
  const module = loadBuiltinModule(id, url);
  if (!StringPrototypeStartsWith(url, 'node:') || !module) {
    throw new ERR_UNKNOWN_BUILTIN_MODULE(url);
  }
  debug(`Loading BuiltinModule ${url}`);
  return module.getESMFacade();
});

// Strategy for loading a JSON file
translators.set('json', function jsonStrategy(url, source) {
  assertBufferSource(source, true, 'load');
  debug(`Loading JSONModule ${url}`);
  const pathname = StringPrototypeStartsWith(url, 'file:') ?
    fileURLToPath(url) : null;
  const shouldCheckAndPopulateCJSModuleCache =
    // We want to involve the CJS loader cache only for `file:` URL with no search query and no hash.
    pathname && !StringPrototypeIncludes(url, '?') && !StringPrototypeIncludes(url, '#');
  let modulePath;
  let module;
  if (shouldCheckAndPopulateCJSModuleCache) {
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
  if (shouldCheckAndPopulateCJSModuleCache) {
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
      loaded: true,
    };
  } catch (err) {
    // TODO (BridgeAR): We could add a NodeCore error that wraps the JSON
    // parse error instead of just manipulating the original error message.
    // That would allow to add further properties and maybe additional
    // debugging information.
    err.message = errPath(url) + ': ' + err.message;
    throw err;
  }
  if (shouldCheckAndPopulateCJSModuleCache) {
    CJSModule._cache[modulePath] = module;
  }
  return new ModuleWrap(url, undefined, ['default'], function() {
    debug(`Parsing JSONModule ${url}`);
    this.setExport('default', module.exports);
  });
});

// Strategy for loading a wasm module
translators.set('wasm', async function(url, source) {
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

  const createDynamicModule = require(
    'internal/modules/esm/create_dynamic_module');
  return createDynamicModule(imports, exports, url, (reflect) => {
    const { exports } = new WebAssembly.Instance(compiled, reflect.imports);
    for (const expt of ObjectKeys(exports)) {
      reflect.exports[expt].set(exports[expt]);
    }
  }).module;
});
