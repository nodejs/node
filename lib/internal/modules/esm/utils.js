'use strict';
const {
  ArrayIsArray,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ArrayPrototypeSome,
  FunctionPrototype,
  JSONParse,
  JSONStringify,
  PromiseResolve,
  PromisePrototypeThen,
  ReflectApply,
  RegExpPrototypeSymbolReplace,
  RegExpPrototypeExec,
  SafePromiseAllReturnArrayLike,
  SafePromiseAllReturnVoid,
  SafeSet,
  SafeWeakMap,
  SafeMap,
  StringPrototypeIncludes,
  StringPrototypeIndexOf,
  StringPrototypeLastIndexOf,
  StringPrototypeReplace,
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
  StringPrototypeEndsWith,
  ObjectCreate,
  ObjectFreeze,
  ObjectSetPrototypeOf,
  ObjectGetOwnPropertyNames,
  ObjectPrototypeHasOwnProperty,
} = primordials;

const {
  ERR_INVALID_ARG_TYPE,
  ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING,
  ERR_INVALID_PACKAGE_CONFIG,
  ERR_UNKNOWN_FILE_EXTENSION,
  ERR_INVALID_MODULE_SPECIFIER,
  ERR_INVALID_PACKAGE_TARGET,
  ERR_MODULE_NOT_FOUND,
  ERR_PACKAGE_IMPORT_NOT_DEFINED,
  ERR_PACKAGE_PATH_NOT_EXPORTED,
  ERR_INVALID_ARG_VALUE,
} = require('internal/errors').codes;

const { validateString } = require('internal/validators');
const assert = require('internal/assert');
const { getOptionValue, getEmbedderOptions } = require('internal/options');
const { BuiltinModule } = require('internal/bootstrap/loaders');

const {
  ModuleWrap,
  setImportModuleDynamicallyCallback,
  setInitializeImportMetaObjectCallback
} = internalBinding('module_wrap');

const { basename, extname, relative } = require('path');
const { decorateErrorStack, filterOwnProperties } = require('internal/util');
const { URL, pathToFileURL, fileURLToPath } = require('internal/url');
const { statSync, Stats } = require('fs');
const { readPackageJSON } = require('internal/modules/helpers');

let debug = require('internal/util/debuglog').debuglog('esm', (fn) => {
  debug = fn;
});

/**
 * @typedef {string | string[] | Record<string, unknown>} Exports
 * @typedef {'module' | 'commonjs'} PackageType
 * @typedef {{
 *   pjsonPath: string,
 *   exports?: ExportConfig,
 *   name?: string,
 *   main?: string,
 *   type?: PackageType,
 * }} PackageConfig
 */

/** @type {Map<string, PackageConfig>} */
const packageJSONCache = new SafeMap();

const callbackMap = new SafeWeakMap();
function setCallbackForWrap(wrap, data) {
  callbackMap.set(wrap, data);
}

const wrapToModuleMap = new SafeWeakMap();

const resolvedPromise = PromiseResolve();

const noop = FunctionPrototype;

let hasPausedEntry = false;

const CJSGlobalLike = [
  'require',
  'module',
  'exports',
  '__filename',
  '__dirname',
];

// The HTML spec has an implied default type of `'javascript'`.
const kImplicitAssertType = 'javascript';

/**
 * Define a map of module formats to import assertion types (the value of
 * `type` in `assert { type: 'json' }`).
 * @type {Map<string, string>}
 */
const formatTypeMap = {
  '__proto__': null,
  'builtin': kImplicitAssertType,
  'commonjs': kImplicitAssertType,
  'json': 'json',
  'module': kImplicitAssertType,
  'wasm': kImplicitAssertType, // It's unclear whether the HTML spec will require an assertion type or not for Wasm; see https://github.com/WebAssembly/esm-integration/issues/42
};

const extensionFormatMap = {
  '__proto__': null,
  '.cjs': 'commonjs',
  '.js': 'module',
  '.json': 'json',
  '.mjs': 'module',
};
const extensionFormatMapInitialized = false;

function getExtensionFormatMap() {
  if (!extensionFormatMapInitialized && getOptionValue('--experimental-wasm-modules')) {
    extensionFormatMap['.wasm'] = 'wasm';
  }
  return extensionFormatMap;
}

let defaultConditions;
function getDefaultConditions() {
  if (defaultConditions === undefined) {
    initializeDefaultConditions();
  }
  return defaultConditions;
}

let defaultConditionsSet;
function getDefaultConditionsSet() {
  if (defaultConditionsSet === undefined) {
    initializeDefaultConditions();
  }
  return defaultConditionsSet;
}

function initializeDefaultConditions() {
  const userConditions = getOptionValue('--conditions');
  const noAddons = getOptionValue('--no-addons');
  const addonConditions = noAddons ? [] : ['node-addons'];

  defaultConditions = ObjectFreeze([
    'node',
    'import',
    ...addonConditions,
    ...userConditions,
  ]);
  defaultConditionsSet = new SafeSet(defaultConditions);
}

/**
 * @param {string[]} [conditions]
 * @returns {Set<string>}
 */
function getConditionsSet(conditions) {
  if (conditions !== undefined && conditions !== getDefaultConditions()) {
    if (!ArrayIsArray(conditions)) {
      throw new ERR_INVALID_ARG_VALUE('conditions', conditions,
                                      'expected an array');
    }
    return new SafeSet(conditions);
  }
  return getDefaultConditionsSet();
}

/**
 * @param {string} path
 * @param {string} specifier
 * @param {string | URL | undefined} base
 * @returns {PackageConfig}
 */
function getPackageConfig(path, specifier, base) {
  const existing = packageJSONCache.get(path);
  if (existing !== undefined) {
    return existing;
  }
  const source = readPackageJSON(path).string;
  if (source === undefined) {
    const packageConfig = {
      pjsonPath: path,
      exists: false,
      main: undefined,
      name: undefined,
      type: 'none',
      exports: undefined,
      imports: undefined,
    };
    packageJSONCache.set(path, packageConfig);
    return packageConfig;
  }

  let packageJSON;
  try {
    packageJSON = JSONParse(source);
  } catch (error) {
    throw new ERR_INVALID_PACKAGE_CONFIG(
      path,
      (base ? `"${specifier}" from ` : '') + fileURLToPath(base || specifier),
      error.message
    );
  }

  let { imports, main, name, type } = filterOwnProperties(packageJSON, ['imports', 'main', 'name', 'type']);
  const exports = ObjectPrototypeHasOwnProperty(packageJSON, 'exports') ? packageJSON.exports : undefined;
  if (typeof imports !== 'object' || imports === null) {
    imports = undefined;
  }
  if (typeof main !== 'string') {
    main = undefined;
  }
  if (typeof name !== 'string') {
    name = undefined;
  }
  // Ignore unknown types for forwards compatibility
  if (type !== 'module' && type !== 'commonjs') {
    type = 'none';
  }

  const packageConfig = {
    pjsonPath: path,
    exists: true,
    main,
    name,
    type,
    exports,
    imports,
  };
  packageJSONCache.set(path, packageConfig);
  return packageConfig;
}


/**
 * @param {URL | string} resolved
 * @returns {PackageConfig}
 */
function getPackageScopeConfig(resolved) {
  let packageJSONUrl = new URL('./package.json', resolved);
  while (true) {
    const packageJSONPath = packageJSONUrl.pathname;
    if (StringPrototypeEndsWith(packageJSONPath, 'node_modules/package.json')) {
      break;
    }
    const packageConfig = getPackageConfig(fileURLToPath(packageJSONUrl), resolved);
    if (packageConfig.exists) {
      return packageConfig;
    }

    const lastPackageJSONUrl = packageJSONUrl;
    packageJSONUrl = new URL('../package.json', packageJSONUrl);

    // Terminates at root where ../package.json equals ../../package.json
    // (can't just check "/package.json" for Windows support).
    if (packageJSONUrl.pathname === lastPackageJSONUrl.pathname) {
      break;
    }
  }
  const packageJSONPath = fileURLToPath(packageJSONUrl);
  const packageConfig = {
    pjsonPath: packageJSONPath,
    exists: false,
    main: undefined,
    name: undefined,
    type: 'none',
    exports: undefined,
    imports: undefined,
  };
  packageJSONCache.set(packageJSONPath, packageConfig);
  return packageConfig;
}

const emittedPackageWarnings = new SafeSet();

function emitTrailingSlashPatternDeprecation(match, pjsonUrl, base) {
  const pjsonPath = fileURLToPath(pjsonUrl);
  if (emittedPackageWarnings.has(pjsonPath + '|' + match))
    return;
  emittedPackageWarnings.add(pjsonPath + '|' + match);
  process.emitWarning(
    `Use of deprecated trailing slash pattern mapping "${match}" in the ` +
    `"exports" field module resolution of the package at ${pjsonPath}${
      base ? ` imported from ${fileURLToPath(base)}` :
        ''}. Mapping specifiers ending in "/" is no longer supported.`,
    'DeprecationWarning',
    'DEP0155'
  );
}

const doubleSlashRegEx = /[/\\][/\\]/;

function emitInvalidSegmentDeprecation(target, request, match, pjsonUrl, internal, base, isTarget) {
  const pjsonPath = fileURLToPath(pjsonUrl);
  const double = RegExpPrototypeExec(doubleSlashRegEx, isTarget ? target : request) !== null;
  process.emitWarning(
    `Use of deprecated ${double ? 'double slash' :
      'leading or trailing slash matching'} resolving "${target}" for module ` +
      `request "${request}" ${request !== match ? `matched to "${match}" ` : ''
      }in the "${internal ? 'imports' : 'exports'}" field module resolution of the package at ${
        pjsonPath}${base ? ` imported from ${fileURLToPath(base)}` : ''}.`,
    'DeprecationWarning',
    'DEP0166'
  );
}

/**
 * @param {URL} url
 * @param {URL} packageJSONUrl
 * @param {string | URL | undefined} base
 * @param {string} main
 * @returns {void}
 */
function emitLegacyIndexDeprecation(url, packageJSONUrl, base, main) {
  const format = defaultGetFormatWithoutErrors(url);
  if (format !== 'module')
    return;
  const path = fileURLToPath(url);
  const pkgPath = fileURLToPath(new URL('.', packageJSONUrl));
  const basePath = fileURLToPath(base);
  if (main)
    process.emitWarning(
      `Package ${pkgPath} has a "main" field set to ${JSONStringify(main)}, ` +
      `excluding the full filename and extension to the resolved file at "${
        StringPrototypeSlice(path, pkgPath.length)}", imported from ${
        basePath}.\n Automatic extension resolution of the "main" field is ` +
      'deprecated for ES modules.',
      'DeprecationWarning',
      'DEP0151'
    );
  else
    process.emitWarning(
      `No "main" or "exports" field defined in the package.json for ${pkgPath
      } resolving the main entry point "${
        StringPrototypeSlice(path, pkgPath.length)}", imported from ${basePath
      }.\nDefault "index" lookups for the main are deprecated for ES modules.`,
      'DeprecationWarning',
      'DEP0151'
    );
}

/**
 * @param {string | URL} path
 * @returns {import('fs').Stats}
 */
const tryStatSync =
  (path) => statSync(path, { throwIfNoEntry: false }) ?? new Stats();

/**
 * @param {string | URL} url
 * @returns {boolean}
 */
function fileExists(url) {
  return statSync(url, { throwIfNoEntry: false })?.isFile() ?? false;
}

/**
 * Legacy CommonJS main resolution:
 * 1. let M = pkg_url + (json main field)
 * 2. TRY(M, M.js, M.json, M.node)
 * 3. TRY(M/index.js, M/index.json, M/index.node)
 * 4. TRY(pkg_url/index.js, pkg_url/index.json, pkg_url/index.node)
 * 5. NOT_FOUND
 * @param {URL} packageJSONUrl
 * @param {PackageConfig} packageConfig
 * @param {string | URL | undefined} base
 * @returns {URL}
 */
function legacyMainResolve(packageJSONUrl, packageConfig, base) {
  let guess;
  if (packageConfig.main !== undefined) {
    // Note: fs check redundances will be handled by Descriptor cache here.
    if (fileExists(guess = new URL(`./${packageConfig.main}`,
                                   packageJSONUrl))) {
      return guess;
    } else if (fileExists(guess = new URL(`./${packageConfig.main}.js`,
                                          packageJSONUrl)));
    else if (fileExists(guess = new URL(`./${packageConfig.main}.json`,
                                        packageJSONUrl)));
    else if (fileExists(guess = new URL(`./${packageConfig.main}.node`,
                                        packageJSONUrl)));
    else if (fileExists(guess = new URL(`./${packageConfig.main}/index.js`,
                                        packageJSONUrl)));
    else if (fileExists(guess = new URL(`./${packageConfig.main}/index.json`,
                                        packageJSONUrl)));
    else if (fileExists(guess = new URL(`./${packageConfig.main}/index.node`,
                                        packageJSONUrl)));
    else guess = undefined;
    if (guess) {
      emitLegacyIndexDeprecation(guess, packageJSONUrl, base,
                                 packageConfig.main);
      return guess;
    }
    // Fallthrough.
  }
  if (fileExists(guess = new URL('./index.js', packageJSONUrl)));
  // So fs.
  else if (fileExists(guess = new URL('./index.json', packageJSONUrl)));
  else if (fileExists(guess = new URL('./index.node', packageJSONUrl)));
  else guess = undefined;
  if (guess) {
    emitLegacyIndexDeprecation(guess, packageJSONUrl, base, packageConfig.main);
    return guess;
  }
  // Not found.
  throw new ERR_MODULE_NOT_FOUND(
    fileURLToPath(new URL('.', packageJSONUrl)), fileURLToPath(base));
}

const encodedSepRegEx = /%2F|%5C/i;

/**
 * @param {string} specifier
 * @param {URL} packageJSONUrl
 * @param {string | URL | undefined} base
 */
function importNotDefined(specifier, packageJSONUrl, base) {
  return new ERR_PACKAGE_IMPORT_NOT_DEFINED(
    specifier, packageJSONUrl && fileURLToPath(new URL('.', packageJSONUrl)),
    fileURLToPath(base));
}

/**
 * @param {string} subpath
 * @param {URL} packageJSONUrl
 * @param {string | URL | undefined} base
 */
function exportsNotFound(subpath, packageJSONUrl, base) {
  return new ERR_PACKAGE_PATH_NOT_EXPORTED(
    fileURLToPath(new URL('.', packageJSONUrl)), subpath,
    base && fileURLToPath(base));
}

/**
 *
 * @param {string} request
 * @param {string} match
 * @param {URL} packageJSONUrl
 * @param {boolean} internal
 * @param {string | URL | undefined} base
 */
function throwInvalidSubpath(request, match, packageJSONUrl, internal, base) {
  const reason = `request is not a valid match in pattern "${match}" for the "${
    internal ? 'imports' : 'exports'}" resolution of ${
    fileURLToPath(packageJSONUrl)}`;
  throw new ERR_INVALID_MODULE_SPECIFIER(request, reason,
                                         base && fileURLToPath(base));
}

function invalidPackageTarget(
  subpath, target, packageJSONUrl, internal, base) {
  if (typeof target === 'object' && target !== null) {
    target = JSONStringify(target, null, '');
  } else {
    target = `${target}`;
  }
  return new ERR_INVALID_PACKAGE_TARGET(
    fileURLToPath(new URL('.', packageJSONUrl)), subpath, target,
    internal, base && fileURLToPath(base));
}

const invalidSegmentRegEx = /(^|\\|\/)((\.|%2e)(\.|%2e)?|(n|%6e|%4e)(o|%6f|%4f)(d|%64|%44)(e|%65|%45)(_|%5f)(m|%6d|%4d)(o|%6f|%4f)(d|%64|%44)(u|%75|%55)(l|%6c|%4c)(e|%65|%45)(s|%73|%53))?(\\|\/|$)/i;
const deprecatedInvalidSegmentRegEx = /(^|\\|\/)((\.|%2e)(\.|%2e)?|(n|%6e|%4e)(o|%6f|%4f)(d|%64|%44)(e|%65|%45)(_|%5f)(m|%6d|%4d)(o|%6f|%4f)(d|%64|%44)(u|%75|%55)(l|%6c|%4c)(e|%65|%45)(s|%73|%53))(\\|\/|$)/i;
const invalidPackageNameRegEx = /^\.|%|\\/;
const patternRegEx = /\*/g;

/**
 *
 * @param {string} target
 * @param {*} subpath
 * @param {*} match
 * @param {*} packageJSONUrl
 * @param {*} base
 * @param {*} pattern
 * @param {*} internal
 * @param {*} isPathMap
 * @param {*} conditions
 * @returns {URL}
 */
function resolvePackageTargetString(
  target,
  subpath,
  match,
  packageJSONUrl,
  base,
  pattern,
  internal,
  isPathMap,
  conditions,
) {

  if (subpath !== '' && !pattern && target[target.length - 1] !== '/')
    throw invalidPackageTarget(match, target, packageJSONUrl, internal, base);

  if (!StringPrototypeStartsWith(target, './')) {
    if (internal && !StringPrototypeStartsWith(target, '../') &&
        !StringPrototypeStartsWith(target, '/')) {
      let isURL = false;
      try {
        new URL(target);
        isURL = true;
      } catch {
        // Continue regardless of error.
      }
      if (!isURL) {
        const exportTarget = pattern ?
          RegExpPrototypeSymbolReplace(patternRegEx, target, () => subpath) :
          target + subpath;
        return packageResolve(
          exportTarget, packageJSONUrl, conditions);
      }
    }
    throw invalidPackageTarget(match, target, packageJSONUrl, internal, base);
  }

  if (RegExpPrototypeExec(invalidSegmentRegEx, StringPrototypeSlice(target, 2)) !== null) {
    if (RegExpPrototypeExec(deprecatedInvalidSegmentRegEx, StringPrototypeSlice(target, 2)) === null) {
      if (!isPathMap) {
        const request = pattern ?
          StringPrototypeReplace(match, '*', () => subpath) :
          match + subpath;
        const resolvedTarget = pattern ?
          RegExpPrototypeSymbolReplace(patternRegEx, target, () => subpath) :
          target;
        emitInvalidSegmentDeprecation(resolvedTarget, request, match, packageJSONUrl, internal, base, true);
      }
    } else {
      throw invalidPackageTarget(match, target, packageJSONUrl, internal, base);
    }
  }

  const resolved = new URL(target, packageJSONUrl);
  const resolvedPath = resolved.pathname;
  const packagePath = new URL('.', packageJSONUrl).pathname;

  if (!StringPrototypeStartsWith(resolvedPath, packagePath))
    throw invalidPackageTarget(match, target, packageJSONUrl, internal, base);

  if (subpath === '') return resolved;

  if (RegExpPrototypeExec(invalidSegmentRegEx, subpath) !== null) {
    const request = pattern ? StringPrototypeReplace(match, '*', () => subpath) : match + subpath;
    if (RegExpPrototypeExec(deprecatedInvalidSegmentRegEx, subpath) === null) {
      if (!isPathMap) {
        const resolvedTarget = pattern ?
          RegExpPrototypeSymbolReplace(patternRegEx, target, () => subpath) :
          target;
        emitInvalidSegmentDeprecation(resolvedTarget, request, match, packageJSONUrl, internal, base, false);
      }
    } else {
      throwInvalidSubpath(request, match, packageJSONUrl, internal, base);
    }
  }

  if (pattern) {
    return new URL(
      RegExpPrototypeSymbolReplace(patternRegEx, resolved.href, () => subpath)
    );
  }

  return new URL(subpath, resolved);
}

/**
 * @param {string} key
 * @returns {boolean}
 */
function isArrayIndex(key) {
  const keyNum = +key;
  if (`${keyNum}` !== key) return false;
  return keyNum >= 0 && keyNum < 0xFFFF_FFFF;
}

/**
 *
 * @param {*} packageJSONUrl
 * @param {string|[string]} target
 * @param {*} subpath
 * @param {*} packageSubpath
 * @param {*} base
 * @param {*} pattern
 * @param {*} internal
 * @param {*} isPathMap
 * @param {*} conditions
 * @returns {URL|null}
 */
function resolvePackageTarget(packageJSONUrl, target, subpath, packageSubpath,
                              base, pattern, internal, isPathMap, conditions) {
  if (typeof target === 'string') {
    return resolvePackageTargetString(
      target, subpath, packageSubpath, packageJSONUrl, base, pattern, internal,
      isPathMap, conditions);
  } else if (ArrayIsArray(target)) {
    if (target.length === 0) {
      return null;
    }

    let lastException;
    for (let i = 0; i < target.length; i++) {
      const targetItem = target[i];
      let resolveResult;
      try {
        resolveResult = resolvePackageTarget(
          packageJSONUrl, targetItem, subpath, packageSubpath, base, pattern,
          internal, isPathMap, conditions);
      } catch (e) {
        lastException = e;
        if (e.code === 'ERR_INVALID_PACKAGE_TARGET') {
          continue;
        }
        throw e;
      }
      if (resolveResult === undefined) {
        continue;
      }
      if (resolveResult === null) {
        lastException = null;
        continue;
      }
      return resolveResult;
    }
    if (lastException === undefined || lastException === null)
      return lastException;
    throw lastException;
  } else if (typeof target === 'object' && target !== null) {
    const keys = ObjectGetOwnPropertyNames(target);
    for (let i = 0; i < keys.length; i++) {
      const key = keys[i];
      if (isArrayIndex(key)) {
        throw new ERR_INVALID_PACKAGE_CONFIG(
          fileURLToPath(packageJSONUrl), base,
          '"exports" cannot contain numeric property keys.');
      }
    }
    for (let i = 0; i < keys.length; i++) {
      const key = keys[i];
      if (key === 'default' || conditions.has(key)) {
        const conditionalTarget = target[key];
        const resolveResult = resolvePackageTarget(
          packageJSONUrl, conditionalTarget, subpath, packageSubpath, base,
          pattern, internal, isPathMap, conditions);
        if (resolveResult === undefined)
          continue;
        return resolveResult;
      }
    }
    return undefined;
  } else if (target === null) {
    return null;
  }
  throw invalidPackageTarget(packageSubpath, target, packageJSONUrl, internal,
                             base);
}

/**
 *
 * @param {Exports} exports
 * @param {URL} packageJSONUrl
 * @param {string | URL | undefined} base
 * @returns {boolean}
 */
function isConditionalExportsMainSugar(exports, packageJSONUrl, base) {
  if (typeof exports === 'string' || ArrayIsArray(exports)) return true;
  if (typeof exports !== 'object' || exports === null) return false;

  const keys = ObjectGetOwnPropertyNames(exports);
  let isConditionalSugar = false;
  let i = 0;
  for (let j = 0; j < keys.length; j++) {
    const key = keys[j];
    const curIsConditionalSugar = key === '' || key[0] !== '.';
    if (i++ === 0) {
      isConditionalSugar = curIsConditionalSugar;
    } else if (isConditionalSugar !== curIsConditionalSugar) {
      throw new ERR_INVALID_PACKAGE_CONFIG(
        fileURLToPath(packageJSONUrl), base,
        '"exports" cannot contain some keys starting with \'.\' and some not.' +
        ' The exports object must either be an object of package subpath keys' +
        ' or an object of main entry condition name keys only.');
    }
  }
  return isConditionalSugar;
}

/**
 * @param {URL} packageJSONUrl
 * @param {string} packageSubpath
 * @param {PackageConfig} packageConfig
 * @param {string | URL | undefined} base
 * @param {Set<string>} conditions
 * @returns {URL}
 */
function packageExportsResolve(
  packageJSONUrl, packageSubpath, packageConfig, base, conditions) {
  let exports = packageConfig.exports;
  if (isConditionalExportsMainSugar(exports, packageJSONUrl, base))
    exports = { '.': exports };

  if (ObjectPrototypeHasOwnProperty(exports, packageSubpath) &&
      !StringPrototypeIncludes(packageSubpath, '*') &&
      !StringPrototypeEndsWith(packageSubpath, '/')) {
    const target = exports[packageSubpath];
    const resolveResult = resolvePackageTarget(
      packageJSONUrl, target, '', packageSubpath, base, false, false, false,
      conditions
    );

    if (resolveResult == null) {
      throw exportsNotFound(packageSubpath, packageJSONUrl, base);
    }

    return resolveResult;
  }

  let bestMatch = '';
  let bestMatchSubpath;
  const keys = ObjectGetOwnPropertyNames(exports);
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    const patternIndex = StringPrototypeIndexOf(key, '*');
    if (patternIndex !== -1 &&
        StringPrototypeStartsWith(packageSubpath,
                                  StringPrototypeSlice(key, 0, patternIndex))) {
      // When this reaches EOL, this can throw at the top of the whole function:
      //
      // if (StringPrototypeEndsWith(packageSubpath, '/'))
      //   throwInvalidSubpath(packageSubpath)
      //
      // To match "imports" and the spec.
      if (StringPrototypeEndsWith(packageSubpath, '/'))
        emitTrailingSlashPatternDeprecation(packageSubpath, packageJSONUrl,
                                            base);
      const patternTrailer = StringPrototypeSlice(key, patternIndex + 1);
      if (packageSubpath.length >= key.length &&
          StringPrototypeEndsWith(packageSubpath, patternTrailer) &&
          patternKeyCompare(bestMatch, key) === 1 &&
          StringPrototypeLastIndexOf(key, '*') === patternIndex) {
        bestMatch = key;
        bestMatchSubpath = StringPrototypeSlice(
          packageSubpath, patternIndex,
          packageSubpath.length - patternTrailer.length);
      }
    }
  }

  if (bestMatch) {
    const target = exports[bestMatch];
    const resolveResult = resolvePackageTarget(
      packageJSONUrl,
      target,
      bestMatchSubpath,
      bestMatch,
      base,
      true,
      false,
      StringPrototypeEndsWith(packageSubpath, '/'),
      conditions);

    if (resolveResult == null) {
      throw exportsNotFound(packageSubpath, packageJSONUrl, base);
    }
    return resolveResult;
  }

  throw exportsNotFound(packageSubpath, packageJSONUrl, base);
}

function patternKeyCompare(a, b) {
  const aPatternIndex = StringPrototypeIndexOf(a, '*');
  const bPatternIndex = StringPrototypeIndexOf(b, '*');
  const baseLenA = aPatternIndex === -1 ? a.length : aPatternIndex + 1;
  const baseLenB = bPatternIndex === -1 ? b.length : bPatternIndex + 1;
  if (baseLenA > baseLenB) return -1;
  if (baseLenB > baseLenA) return 1;
  if (aPatternIndex === -1) return 1;
  if (bPatternIndex === -1) return -1;
  if (a.length > b.length) return -1;
  if (b.length > a.length) return 1;
  return 0;
}

/**
 * @param {string} name
 * @param {string | URL | undefined} base
 * @param {Set<string>} conditions
 * @returns {URL}
 */
function packageImportsResolve(name, base, conditions) {
  if (name === '#' || StringPrototypeStartsWith(name, '#/') ||
      StringPrototypeEndsWith(name, '/')) {
    const reason = 'is not a valid internal imports specifier name';
    throw new ERR_INVALID_MODULE_SPECIFIER(name, reason, fileURLToPath(base));
  }
  let packageJSONUrl;
  const packageConfig = getPackageScopeConfig(base);
  if (packageConfig.exists) {
    packageJSONUrl = pathToFileURL(packageConfig.pjsonPath);
    const imports = packageConfig.imports;
    if (imports) {
      if (ObjectPrototypeHasOwnProperty(imports, name) &&
          !StringPrototypeIncludes(name, '*')) {
        const resolveResult = resolvePackageTarget(
          packageJSONUrl, imports[name], '', name, base, false, true, false,
          conditions
        );
        if (resolveResult != null) {
          return resolveResult;
        }
      } else {
        let bestMatch = '';
        let bestMatchSubpath;
        const keys = ObjectGetOwnPropertyNames(imports);
        for (let i = 0; i < keys.length; i++) {
          const key = keys[i];
          const patternIndex = StringPrototypeIndexOf(key, '*');
          if (patternIndex !== -1 &&
              StringPrototypeStartsWith(name,
                                        StringPrototypeSlice(key, 0,
                                                             patternIndex))) {
            const patternTrailer = StringPrototypeSlice(key, patternIndex + 1);
            if (name.length >= key.length &&
                StringPrototypeEndsWith(name, patternTrailer) &&
                patternKeyCompare(bestMatch, key) === 1 &&
                StringPrototypeLastIndexOf(key, '*') === patternIndex) {
              bestMatch = key;
              bestMatchSubpath = StringPrototypeSlice(
                name, patternIndex, name.length - patternTrailer.length);
            }
          }
        }

        if (bestMatch) {
          const target = imports[bestMatch];
          const resolveResult = resolvePackageTarget(packageJSONUrl, target,
                                                     bestMatchSubpath,
                                                     bestMatch, base, true,
                                                     true, false, conditions);
          if (resolveResult != null) {
            return resolveResult;
          }
        }
      }
    }
  }
  throw importNotDefined(name, packageJSONUrl, base);
}

/**
 * @param {string} specifier
 * @param {string | URL | undefined} base
 * @returns {{ packageName: string, packageSubpath: string, isScoped: boolean }}
 */
function parsePackageName(specifier, base) {
  let separatorIndex = StringPrototypeIndexOf(specifier, '/');
  let validPackageName = true;
  let isScoped = false;
  if (specifier[0] === '@') {
    isScoped = true;
    if (separatorIndex === -1 || specifier.length === 0) {
      validPackageName = false;
    } else {
      separatorIndex = StringPrototypeIndexOf(
        specifier, '/', separatorIndex + 1);
    }
  }

  const packageName = separatorIndex === -1 ?
    specifier : StringPrototypeSlice(specifier, 0, separatorIndex);

  // Package name cannot have leading . and cannot have percent-encoding or
  // \\ separators.
  if (RegExpPrototypeExec(invalidPackageNameRegEx, packageName) !== null)
    validPackageName = false;

  if (!validPackageName) {
    throw new ERR_INVALID_MODULE_SPECIFIER(
      specifier, 'is not a valid package name', fileURLToPath(base));
  }

  const packageSubpath = '.' + (separatorIndex === -1 ? '' :
    StringPrototypeSlice(specifier, separatorIndex));

  return { packageName, packageSubpath, isScoped };
}

/**
 * @param {string} specifier
 * @param {string | URL | undefined} base
 * @param {Set<string>} conditions
 * @returns {resolved: URL, format? : string}
 */
function packageResolve(specifier, base, conditions) {
  if (BuiltinModule.canBeRequiredByUsers(specifier) &&
      BuiltinModule.canBeRequiredWithoutScheme(specifier)) {
    return new URL('node:' + specifier);
  }

  const { packageName, packageSubpath, isScoped } =
    parsePackageName(specifier, base);

  // ResolveSelf
  const packageConfig = getPackageScopeConfig(base);
  if (packageConfig.exists) {
    const packageJSONUrl = pathToFileURL(packageConfig.pjsonPath);
    if (packageConfig.name === packageName &&
        packageConfig.exports !== undefined && packageConfig.exports !== null) {
      return packageExportsResolve(
        packageJSONUrl, packageSubpath, packageConfig, base, conditions);
    }
  }

  let packageJSONUrl =
    new URL('./node_modules/' + packageName + '/package.json', base);
  let packageJSONPath = fileURLToPath(packageJSONUrl);
  let lastPath;
  do {
    const stat = tryStatSync(StringPrototypeSlice(packageJSONPath, 0,
                                                  packageJSONPath.length - 13));
    if (!stat.isDirectory()) {
      lastPath = packageJSONPath;
      packageJSONUrl = new URL((isScoped ?
        '../../../../node_modules/' : '../../../node_modules/') +
        packageName + '/package.json', packageJSONUrl);
      packageJSONPath = fileURLToPath(packageJSONUrl);
      continue;
    }

    // Package match.
    const packageConfig = getPackageConfig(packageJSONPath, specifier, base);
    if (packageConfig.exports !== undefined && packageConfig.exports !== null) {
      return packageExportsResolve(
        packageJSONUrl, packageSubpath, packageConfig, base, conditions);
    }
    if (packageSubpath === '.') {
      return legacyMainResolve(
        packageJSONUrl,
        packageConfig,
        base
      );
    }

    return new URL(packageSubpath, packageJSONUrl);
    // Cross-platform root check.
  } while (packageJSONPath.length !== lastPath.length);

  // eslint can't handle the above code.
  // eslint-disable-next-line no-unreachable
  throw new ERR_MODULE_NOT_FOUND(packageName, fileURLToPath(base));
}

/**
 * @param {URL} url
 * @returns {PackageType}
 */
function getPackageType(url) {
  const packageConfig = getPackageScopeConfig(url);
  return packageConfig.type;
}

/**
 * @param {string} mime
 * @returns {string | null}
 */
function mimeToFormat(mime) {
  if (
    RegExpPrototypeExec(
      /\s*(text|application)\/javascript\s*(;\s*charset=utf-?8\s*)?/i,
      mime
    ) !== null
  ) return 'module';
  if (mime === 'application/json') return 'json';
  if (getOptionValue('--experimental-wasm-modules') && mime === 'application/wasm') {
    return 'wasm';
  }
  return null;
}

const protocolHandlers = {
  '__proto__': null,
  'data:': getDataProtocolModuleFormat,
  'file:': getFileProtocolModuleFormat,
  'http:': getHttpProtocolModuleFormat,
  'https:': getHttpProtocolModuleFormat,
  'node:'() { return 'builtin'; },
};

/**
 * @param {URL} parsed
 * @returns {string | null}
 */
function getDataProtocolModuleFormat(parsed) {
  const { 1: mime } = RegExpPrototypeExec(
    /^([^/]+\/[^;,]+)(?:[^,]*?)(;base64)?,/,
    parsed.pathname,
  ) || [ null, null, null ];

  return mimeToFormat(mime);
}

/**
 * @param {URL} url
 * @param {{parentURL: string}} context
 * @param {boolean} ignoreErrors
 * @returns {string}
 */
function getFileProtocolModuleFormat(url, context, ignoreErrors) {
  const filepath = fileURLToPath(url);
  const ext = extname(filepath);
  if (ext === '.js') {
    return getPackageType(url) === 'module' ? 'module' : 'commonjs';
  }

  const format = getExtensionFormatMap()[ext];
  if (format) return format;

  // Explicit undefined return indicates load hook should rerun format check
  if (ignoreErrors) { return undefined; }
  let suggestion = '';
  if (getPackageType(url) === 'module' && ext === '') {
    const config = getPackageScopeConfig(url);
    const fileBasename = basename(filepath);
    const relativePath = StringPrototypeSlice(relative(config.pjsonPath, filepath), 1);
    suggestion = 'Loading extensionless files is not supported inside of ' +
      '"type":"module" package.json contexts. The package.json file ' +
      `${config.pjsonPath} caused this "type":"module" context. Try ` +
      `changing ${filepath} to have a file extension. Note the "bin" ` +
      'field of package.json can point to a file with an extension, for example ' +
      `{"type":"module","bin":{"${fileBasename}":"${relativePath}.js"}}`;
  }
  throw new ERR_UNKNOWN_FILE_EXTENSION(ext, filepath, suggestion);
}

/**
 * @param {URL} url
 * @param {{parentURL: string}} context
 * @returns {Promise<string> | undefined} only works when enabled
 */
function getHttpProtocolModuleFormat(url, context) {
  if (getOptionValue('--experimental-network-imports')) {
    const { fetchModule } = require('internal/modules/esm/fetch_module');
    return PromisePrototypeThen(
      PromiseResolve(fetchModule(url, context)),
      (entry) => {
        return mimeToFormat(entry.headers['content-type']);
      }
    );
  }
}

/**
 * @param {URL | URL['href']} url
 * @param {{parentURL: string}} context
 * @returns {Promise<string> | string | undefined} only works when enabled
 */
function defaultGetFormatWithoutErrors(url, context) {
  const parsed = new URL(url);
  if (!ObjectPrototypeHasOwnProperty(protocolHandlers, parsed.protocol))
    return null;
  return protocolHandlers[parsed.protocol](parsed, context, true);
}

/**
 * @param {URL | URL['href']} url
 * @param {{parentURL: string}} context
 * @returns {Promise<string> | string | undefined} only works when enabled
 */
function defaultGetFormat(url, context) {
  const parsed = new URL(url);
  return ObjectPrototypeHasOwnProperty(protocolHandlers, parsed.protocol) ?
    protocolHandlers[parsed.protocol](parsed, context, false) :
    null;
}

function isCommonJSGlobalLikeNotDefinedError(errorMessage) {
  return ArrayPrototypeSome(
    CJSGlobalLike,
    (globalLike) => errorMessage === `${globalLike} is not defined`
  );
}

/* A ModuleJob tracks the loading of a single Module, and the ModuleJobs of
 * its dependencies, over time. */
class ModuleJob {
  // `loader` is the Loader instance used for loading dependencies.
  // `moduleProvider` is a function
  constructor(loader, url, importAssertions = ObjectCreate(null),
              moduleProvider, isMain, inspectBrk) {
    this.loader = loader;
    this.importAssertions = importAssertions;
    this.isMain = isMain;
    this.inspectBrk = inspectBrk;

    this.module = undefined;
    // Expose the promise to the ModuleWrap directly for linking below.
    // `this.module` is also filled in below.
    this.modulePromise = ReflectApply(moduleProvider, loader, [url, isMain]);

    // Wait for the ModuleWrap instance being linked with all dependencies.
    const link = async () => {
      this.module = await this.modulePromise;
      assert(this.module instanceof ModuleWrap);

      // Explicitly keeping track of dependency jobs is needed in order
      // to flatten out the dependency graph below in `_instantiate()`,
      // so that circular dependencies can't cause a deadlock by two of
      // these `link` callbacks depending on each other.
      const dependencyJobs = [];
      const promises = this.module.link(async (specifier, assertions) => {
        const jobPromise = this.loader.getModuleJob(specifier, url, assertions);
        ArrayPrototypePush(dependencyJobs, jobPromise);
        const job = await jobPromise;
        return job.modulePromise;
      });

      if (promises !== undefined)
        await SafePromiseAllReturnVoid(promises);

      return SafePromiseAllReturnArrayLike(dependencyJobs);
    };
    // Promise for the list of all dependencyJobs.
    this.linked = link();
    // This promise is awaited later anyway, so silence
    // 'unhandled rejection' warnings.
    PromisePrototypeThen(this.linked, undefined, noop);

    // instantiated == deep dependency jobs wrappers are instantiated,
    // and module wrapper is instantiated.
    this.instantiated = undefined;
  }

  instantiate() {
    if (this.instantiated === undefined) {
      this.instantiated = this._instantiate();
    }
    return this.instantiated;
  }

  async _instantiate() {
    const jobsInGraph = new SafeSet();
    const addJobsToDependencyGraph = async (moduleJob) => {
      if (jobsInGraph.has(moduleJob)) {
        return;
      }
      jobsInGraph.add(moduleJob);
      const dependencyJobs = await moduleJob.linked;
      return SafePromiseAllReturnVoid(dependencyJobs, addJobsToDependencyGraph);
    };
    await addJobsToDependencyGraph(this);

    try {
      if (!hasPausedEntry && this.inspectBrk) {
        hasPausedEntry = true;
        const initWrapper = internalBinding('inspector').callAndPauseOnStart;
        initWrapper(this.module.instantiate, this.module);
      } else {
        this.module.instantiate();
      }
    } catch (e) {
      decorateErrorStack(e);
      const {
        getSourceMapsEnabled,
      } = require('internal/source_map/source_map_cache');
      // TODO(@bcoe): Add source map support to exception that occurs as result
      // of missing named export. This is currently not possible because
      // stack trace originates in module_job, not the file itself. A hidden
      // symbol with filename could be set in node_errors.cc to facilitate this.
      if (!getSourceMapsEnabled() &&
          StringPrototypeIncludes(e.message,
                                  ' does not provide an export named')) {
        const splitStack = StringPrototypeSplit(e.stack, '\n');
        const parentFileUrl = RegExpPrototypeSymbolReplace(
          /:\d+$/,
          splitStack[0],
          ''
        );
        const { 1: childSpecifier, 2: name } = RegExpPrototypeExec(
          /module '(.*)' does not provide an export named '(.+)'/,
          e.message);
        const { url: childFileURL } = await this.loader.resolve(
          childSpecifier, parentFileUrl,
        );
        let format;
        try {
          // This might throw for non-CommonJS modules because we aren't passing
          // in the import assertions and some formats require them; but we only
          // care about CommonJS for the purposes of this error message.
          ({ format } =
            await this.loader.load(childFileURL));
        } catch {
          // Continue regardless of error.
        }

        if (format === 'commonjs') {
          const importStatement = splitStack[1];
          // TODO(@ctavan): The original error stack only provides the single
          // line which causes the error. For multi-line import statements we
          // cannot generate an equivalent object destructuring assignment by
          // just parsing the error stack.
          const oneLineNamedImports = RegExpPrototypeExec(/{.*}/, importStatement);
          const destructuringAssignment = oneLineNamedImports &&
            RegExpPrototypeSymbolReplace(/\s+as\s+/g, oneLineNamedImports, ': ');
          e.message = `Named export '${name}' not found. The requested module` +
            ` '${childSpecifier}' is a CommonJS module, which may not support` +
            ' all module.exports as named exports.\nCommonJS modules can ' +
            'always be imported via the default export, for example using:' +
            `\n\nimport pkg from '${childSpecifier}';\n${
              destructuringAssignment ?
                `const ${destructuringAssignment} = pkg;\n` : ''}`;
          const newStack = StringPrototypeSplit(e.stack, '\n');
          newStack[3] = `SyntaxError: ${e.message}`;
          e.stack = ArrayPrototypeJoin(newStack, '\n');
        }
      }
      throw e;
    }

    for (const dependencyJob of jobsInGraph) {
      // Calling `this.module.instantiate()` instantiates not only the
      // ModuleWrap in this module, but all modules in the graph.
      dependencyJob.instantiated = resolvedPromise;
    }
  }

  async run() {
    await this.instantiate();
    const timeout = -1;
    const breakOnSigint = false;
    try {
      await this.module.evaluate(timeout, breakOnSigint);
    } catch (e) {
      if (e?.name === 'ReferenceError' &&
          isCommonJSGlobalLikeNotDefinedError(e.message)) {
        e.message += ' in ES module scope';

        if (StringPrototypeStartsWith(e.message, 'require ')) {
          e.message += ', you can use import instead';
        }

        const packageConfig =
          StringPrototypeStartsWith(this.module.url, 'file://') &&
            RegExpPrototypeExec(/\.js(\?[^#]*)?(#.*)?$/, this.module.url) !== null &&
            getPackageScopeConfig(this.module.url);
        if (packageConfig.type === 'module') {
          e.message +=
            '\nThis file is being treated as an ES module because it has a ' +
            `'.js' file extension and '${packageConfig.pjsonPath}' contains ` +
            '"type": "module". To treat it as a CommonJS script, rename it ' +
            'to use the \'.cjs\' file extension.';
        }
      }
      throw e;
    }
    return { __proto__: null, module: this.module };
  }
}
ObjectSetPrototypeOf(ModuleJob.prototype, null);

// Tracks the state of the loader-level module cache
class ModuleMap extends SafeMap {
  constructor(i) { super(i); } // eslint-disable-line no-useless-constructor
  get(url, type = kImplicitAssertType) {
    validateString(url, 'url');
    validateString(type, 'type');
    return super.get(url)?.[type];
  }
  set(url, type = kImplicitAssertType, job) {
    validateString(url, 'url');
    validateString(type, 'type');

    if (job instanceof ModuleJob !== true &&
        typeof job !== 'function') {
      throw new ERR_INVALID_ARG_TYPE('job', 'ModuleJob', job);
    }
    debug(`Storing ${url} (${
      type === kImplicitAssertType ? 'implicit type' : type
    }) in ModuleMap`);
    const cachedJobsForUrl = super.get(url) ?? ObjectCreate(null);
    cachedJobsForUrl[type] = job;
    return super.set(url, cachedJobsForUrl);
  }
  has(url, type = kImplicitAssertType) {
    validateString(url, 'url');
    validateString(type, 'type');
    return super.get(url)?.[type] !== undefined;
  }
}

function initializeImportMetaObject(wrap, meta) {
  if (callbackMap.has(wrap)) {
    const { initializeImportMeta } = callbackMap.get(wrap);
    if (initializeImportMeta !== undefined) {
      initializeImportMeta(meta, wrapToModuleMap.get(wrap) || wrap);
    }
  }
}

async function importModuleDynamicallyCallback(wrap, specifier, assertions) {
  if (callbackMap.has(wrap)) {
    const { importModuleDynamically } = callbackMap.get(wrap);
    if (importModuleDynamically !== undefined) {
      return importModuleDynamically(
        specifier, wrapToModuleMap.get(wrap) || wrap, assertions);
    }
  }
  throw new ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING();
}

function initializeESM() {
  if (getEmbedderOptions().shouldNotRegisterESMLoader) return;
  initializeDefaultConditions();
  setInitializeImportMetaObjectCallback(initializeImportMetaObject);
  setImportModuleDynamicallyCallback(importModuleDynamicallyCallback);
}

module.exports = {
  wrapToModuleMap,
  setCallbackForWrap,
  initializeESM,
  ModuleMap,
  ModuleJob,
  getExtensionFormatMap,
  defaultGetFormat,
  defaultGetFormatWithoutErrors,
  packageExportsResolve,
  packageImportsResolve,
  encodedSepRegEx,
  tryStatSync,
  formatTypeMap,
  kImplicitAssertType,
  packageResolve,
  getDefaultConditions,
  getConditionsSet,
};
