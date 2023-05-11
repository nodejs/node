'use strict';

const {
  ArrayPrototypeMap,
  Function,
  JSONParse,
  ObjectGetPrototypeOf,
  ObjectPrototypeHasOwnProperty,
  ObjectKeys,
  ReflectApply,
  SafeArrayIterator,
  SafeMap,
  SafeSet,
  StringPrototypeReplaceAll,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  SyntaxErrorPrototype,
  globalThis: { WebAssembly },
} = primordials;

let _TYPES = null;
function lazyTypes() {
  if (_TYPES !== null) return _TYPES;
  return _TYPES = require('internal/util/types');
}

const { readFileSync } = require('fs');
const { dirname, join } = require('path');
const {
  hasEsmSyntax,
  loadBuiltinModule,
  stripBOM,
} = require('internal/modules/helpers');
const {
  Module: CJSModule,
} = require('internal/modules/cjs/loader');
const { fileURLToPath, pathToFileURL, URL } = require('internal/url');
let debug = require('internal/util/debuglog').debuglog('esm', (fn) => {
  debug = fn;
});
const { emitExperimentalWarning, kEmptyObject } = require('internal/util');
const {
  ERR_UNKNOWN_BUILTIN_MODULE,
  ERR_INVALID_RETURN_PROPERTY_VALUE,
} = require('internal/errors').codes;
const { maybeCacheSourceMap } = require('internal/source_map/source_map_cache');
const moduleWrap = internalBinding('module_wrap');
const { ModuleWrap } = moduleWrap;
const asyncESM = require('internal/process/esm_loader');
const { emitWarningSync } = require('internal/process/warning');

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

const translators = new SafeMap();
exports.translators = translators;
exports.enrichCJSError = enrichCJSError;

let DECODER = null;
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

async function importModuleDynamically(specifier, { url }, assertions) {
  return asyncESM.esmLoader.import(specifier, url, assertions);
}

// Strategy for loading a standard JavaScript module.
translators.set('module', async function moduleStrategy(url, source, isMain) {
  assertBufferSource(source, true, 'load');
  source = stringify(source);
  maybeCacheSourceMap(url, source);
  debug(`Translating StandardModule ${url}`);
  const module = new ModuleWrap(url, undefined, source, 0, 0);
  const { setCallbackForWrap } = require('internal/modules/esm/utils');
  setCallbackForWrap(module, {
    initializeImportMeta: (meta, wrap) => this.importMetaInitialize(meta, { url }),
    importModuleDynamically,
  });
  return module;
});

/**
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

const cjsCache = new Map();
function loadCJS(url, source) {
  if (cjsCache.has(url)) return cjsCache.get(url).exports;

  debug(`Loading CJSModule ${url}`);

  const compiledWrapper = Function('exports', 'require', 'module', '__filename', '__dirname', source);

  // TODO: make module object compatible with CJS Module class
  const module = { exports: {} };
  const filename = fileURLToPath(new URL(url));
  const __dirname = dirname(filename);
  // TODO: add missing properties on the require function
  // eslint-disable-next-line func-name-matching,func-style
  const requireFn = function require(spec) {
    // TODO: add support for absolute paths
    if (spec[0] === '.') {
      spec = pathToFileURL(join(__dirname, spec));
    }
    // TODO: add support for JSON imports
    const job = asyncESM.esmLoader.getModuleJobSync(spec, url, kEmptyObject);
    job.runSync();
    return cjsCache.get(job.url).exports;
  };

  cjsCache.set(url, module);
  ReflectApply(compiledWrapper, module.exports,
               [module.exports, requireFn, module, filename, __dirname]);

  return module.exports;
}

translators.set('require-commonjs', (url, source, isMain) => {
  return new ModuleWrap(url, undefined, ['default'], function() {
    const exports = loadCJS(url, source);
    this.setExport('default', exports);
  });
});

// Strategy for loading a node-style CommonJS module
const isWindows = process.platform === 'win32';
translators.set('commonjs', async function commonjsStrategy(url, source,
                                                            isMain) {
  debug(`Translating CJSModule ${url}`);

  const filename = fileURLToPath(new URL(url));

  if (!cjsParse) await initCJSParse();
  const { exportNames } = cjsPreparseModuleExports(filename);
  const namesWithDefault = exportNames.has('default') ?
    [...exportNames] : ['default', ...exportNames];

  return new ModuleWrap(url, undefined, namesWithDefault, function() {
    const exports = loadCJS(url, source);

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
});

function cjsPreparseModuleExports(filename) {
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
  // cjsParseCache.set(module, { source, exportNames });

  // TODO: add back support for reexports
  // if (reexports.length) {
  //   module.filename = filename;
  //   module.paths = CJSModule._nodeModulePaths(module.path);
  // }
  // ArrayPrototypeForEach(reexports, (reexport) => {
  //   let resolved;
  //   try {
  //     resolved = CJSModule._resolveFilename(reexport, module);
  //   } catch {
  //     return;
  //   }
  //   const ext = extname(resolved);
  //   if ((ext === '.js' || ext === '.cjs' || !CJSModule._extensions[ext]) &&
  //       isAbsolute(resolved)) {
  //     const { exportNames: reexportNames } = cjsPreparseModuleExports(resolved);
  //     for (const name of reexportNames)
  //       exportNames.add(name);
  //   }
  // });

  return { exportNames };
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
translators.set('json', async function jsonStrategy(url, source) {
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
    // TODO: remove CJS loader from here as well.
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
  if (pathname) {
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
    for (const expt of ObjectKeys(exports))
      reflect.exports[expt].set(exports[expt]);
  }).module;
});
