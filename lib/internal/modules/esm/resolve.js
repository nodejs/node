'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeConcat,
  ArrayPrototypeJoin,
  ArrayPrototypeShift,
  JSONParse,
  JSONStringify,
  ObjectFreeze,
  ObjectGetOwnPropertyNames,
  ObjectPrototypeHasOwnProperty,
  RegExp,
  RegExpPrototypeExec,
  RegExpPrototypeSymbolReplace,
  RegExpPrototypeTest,
  SafeMap,
  SafeSet,
  String,
  StringPrototypeEndsWith,
  StringPrototypeIncludes,
  StringPrototypeIndexOf,
  StringPrototypeLastIndexOf,
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
} = primordials;
const internalFS = require('internal/fs/utils');
const { NativeModule } = require('internal/bootstrap/loaders');
const {
  realpathSync,
  statSync,
  Stats,
} = require('fs');
const { getOptionValue } = require('internal/options');
// Do not eagerly grab .manifest, it may be in TDZ
const policy = getOptionValue('--experimental-policy') ?
  require('internal/process/policy') :
  null;
const { sep, relative, resolve } = require('path');
const preserveSymlinks = getOptionValue('--preserve-symlinks');
const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
const experimentalNetworkImports =
  getOptionValue('--experimental-network-imports');
const typeFlag = getOptionValue('--input-type');
const pendingDeprecation = getOptionValue('--pending-deprecation');
const { URL, pathToFileURL, fileURLToPath } = require('internal/url');
const {
  ERR_INPUT_TYPE_NOT_ALLOWED,
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_MODULE_SPECIFIER,
  ERR_INVALID_PACKAGE_CONFIG,
  ERR_INVALID_PACKAGE_TARGET,
  ERR_MANIFEST_DEPENDENCY_MISSING,
  ERR_MODULE_NOT_FOUND,
  ERR_PACKAGE_IMPORT_NOT_DEFINED,
  ERR_PACKAGE_PATH_NOT_EXPORTED,
  ERR_UNSUPPORTED_DIR_IMPORT,
  ERR_NETWORK_IMPORT_DISALLOWED,
  ERR_UNSUPPORTED_ESM_URL_SCHEME,
} = require('internal/errors').codes;
const { Module: CJSModule } = require('internal/modules/cjs/loader');

const packageJsonReader = require('internal/modules/package_json_reader');
const userConditions = getOptionValue('--conditions');
const noAddons = getOptionValue('--no-addons');
const addonConditions = noAddons ? [] : ['node-addons'];

const DEFAULT_CONDITIONS = ObjectFreeze([
  'node',
  'import',
  ...addonConditions,
  ...userConditions,
]);

const DEFAULT_CONDITIONS_SET = new SafeSet(DEFAULT_CONDITIONS);

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

const emittedPackageWarnings = new SafeSet();

/**
 * @param {string} match
 * @param {URL} pjsonUrl
 * @param {boolean} isExports
 * @param {string | URL | undefined} base
 * @returns {void}
 */
function emitFolderMapDeprecation(match, pjsonUrl, isExports, base) {
  const pjsonPath = fileURLToPath(pjsonUrl);

  if (emittedPackageWarnings.has(pjsonPath + '|' + match))
    return;
  emittedPackageWarnings.add(pjsonPath + '|' + match);
  process.emitWarning(
    `Use of deprecated folder mapping "${match}" in the ${isExports ?
      '"exports"' : '"imports"'} field module resolution of the package at ${
      pjsonPath}${base ? ` imported from ${fileURLToPath(base)}` : ''}.\n` +
      `Update this package.json to use a subpath pattern like "${match}*".`,
    'DeprecationWarning',
    'DEP0148'
  );
}

function emitTrailingSlashPatternDeprecation(match, pjsonUrl, isExports, base) {
  if (!pendingDeprecation) return;
  const pjsonPath = fileURLToPath(pjsonUrl);
  if (emittedPackageWarnings.has(pjsonPath + '|' + match))
    return;
  emittedPackageWarnings.add(pjsonPath + '|' + match);
  process.emitWarning(
    `Use of deprecated trailing slash pattern mapping "${match}" in the ${
      isExports ? '"exports"' : '"imports"'} field module resolution of the ` +
      `package at ${pjsonPath}${base ? ` imported from ${fileURLToPath(base)}` :
        ''}. Mapping specifiers ending in "/" is no longer supported.`,
    'DeprecationWarning',
    'DEP0155'
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
 * @param {string[]} [conditions]
 * @returns {Set<string>}
 */
function getConditionsSet(conditions) {
  if (conditions !== undefined && conditions !== DEFAULT_CONDITIONS) {
    if (!ArrayIsArray(conditions)) {
      throw new ERR_INVALID_ARG_VALUE('conditions', conditions,
                                      'expected an array');
    }
    return new SafeSet(conditions);
  }
  return DEFAULT_CONDITIONS_SET;
}

const realpathCache = new SafeMap();
const packageJSONCache = new SafeMap(); /* string -> PackageConfig */

/**
 * @param {string | URL} path
 * @returns {import('fs').Stats}
 */
const tryStatSync =
  (path) => statSync(path, { throwIfNoEntry: false }) ?? new Stats();

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
  const source = packageJsonReader.read(path).string;
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

  let { imports, main, name, type } = packageJSON;
  const { exports } = packageJSON;
  if (typeof imports !== 'object' || imports === null) imports = undefined;
  if (typeof main !== 'string') main = undefined;
  if (typeof name !== 'string') name = undefined;
  // Ignore unknown types for forwards compatibility
  if (type !== 'module' && type !== 'commonjs') type = 'none';

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
    if (StringPrototypeEndsWith(packageJSONPath, 'node_modules/package.json'))
      break;
    const packageConfig = getPackageConfig(fileURLToPath(packageJSONUrl),
                                           resolved);
    if (packageConfig.exists) return packageConfig;

    const lastPackageJSONUrl = packageJSONUrl;
    packageJSONUrl = new URL('../package.json', packageJSONUrl);

    // Terminates at root where ../package.json equals ../../package.json
    // (can't just check "/package.json" for Windows support).
    if (packageJSONUrl.pathname === lastPackageJSONUrl.pathname) break;
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

/**
 * @param {URL} search
 * @returns {URL | undefined}
 */
function resolveExtensionsWithTryExactName(search) {
  if (fileExists(search)) return search;
  return resolveExtensions(search);
}

const extensions = ['.js', '.json', '.node', '.mjs'];

/**
 * @param {URL} search
 * @returns {URL | undefined}
 */
function resolveExtensions(search) {
  for (let i = 0; i < extensions.length; i++) {
    const extension = extensions[i];
    const guess = new URL(`${search.pathname}${extension}`, search);
    if (fileExists(guess)) return guess;
  }
  return undefined;
}

/**
 * @param {URL} search
 * @returns {URL | undefined}
 */
function resolveDirectoryEntry(search) {
  const dirPath = fileURLToPath(search);
  const pkgJsonPath = resolve(dirPath, 'package.json');
  if (fileExists(pkgJsonPath)) {
    const pkgJson = packageJsonReader.read(pkgJsonPath);
    if (pkgJson.containsKeys) {
      const { main } = JSONParse(pkgJson.string);
      if (main != null) {
        const mainUrl = pathToFileURL(resolve(dirPath, main));
        return resolveExtensionsWithTryExactName(mainUrl);
      }
    }
  }
  return resolveExtensions(new URL('index', search));
}

const encodedSepRegEx = /%2F|%5C/i;
let experimentalSpecifierResolutionWarned = false;
/**
 * @param {URL} resolved
 * @param {string | URL | undefined} base
 * @param {boolean} preserveSymlinks
 * @returns {URL | undefined}
 */
function finalizeResolution(resolved, base, preserveSymlinks) {
  if (RegExpPrototypeTest(encodedSepRegEx, resolved.pathname))
    throw new ERR_INVALID_MODULE_SPECIFIER(
      resolved.pathname, 'must not include encoded "/" or "\\" characters',
      fileURLToPath(base));

  let path = fileURLToPath(resolved);
  if (getOptionValue('--experimental-specifier-resolution') === 'node') {
    if (!experimentalSpecifierResolutionWarned) {
      process.emitWarning(
        'The Node.js specifier resolution flag is experimental. It could change or be removed at any time.',
        'ExperimentalWarning');
      experimentalSpecifierResolutionWarned = true;
    }

    let file = resolveExtensionsWithTryExactName(resolved);

    // Directory
    if (file === undefined) {
      file = StringPrototypeEndsWith(path, '/') ?
        (resolveDirectoryEntry(resolved) || resolved) : resolveDirectoryEntry(new URL(`${resolved}/`));

      if (file === resolved) return file;

      if (file === undefined) {
        throw new ERR_MODULE_NOT_FOUND(
          resolved.pathname, fileURLToPath(base), 'module');
      }
    }

    path = file;
  }

  const stats = tryStatSync(StringPrototypeEndsWith(path, '/') ?
    StringPrototypeSlice(path, -1) : path);
  if (stats.isDirectory()) {
    const err = new ERR_UNSUPPORTED_DIR_IMPORT(path, fileURLToPath(base));
    err.url = String(resolved);
    throw err;
  } else if (!stats.isFile()) {
    throw new ERR_MODULE_NOT_FOUND(
      path || resolved.pathname, base && fileURLToPath(base), 'module');
  }

  if (!preserveSymlinks) {
    const real = realpathSync(path, {
      [internalFS.realpathCacheKey]: realpathCache
    });
    const { search, hash } = resolved;
    resolved =
        pathToFileURL(real + (StringPrototypeEndsWith(path, sep) ? '/' : ''));
    resolved.search = search;
    resolved.hash = hash;
  }

  return resolved;
}

/**
 * @param {string} specifier
 * @param {URL} packageJSONUrl
 * @param {string | URL | undefined} base
 */
function throwImportNotDefined(specifier, packageJSONUrl, base) {
  throw new ERR_PACKAGE_IMPORT_NOT_DEFINED(
    specifier, packageJSONUrl && fileURLToPath(new URL('.', packageJSONUrl)),
    fileURLToPath(base));
}

/**
 * @param {string} subpath
 * @param {URL} packageJSONUrl
 * @param {string | URL | undefined} base
 */
function throwExportsNotFound(subpath, packageJSONUrl, base) {
  throw new ERR_PACKAGE_PATH_NOT_EXPORTED(
    fileURLToPath(new URL('.', packageJSONUrl)), subpath,
    base && fileURLToPath(base));
}

/**
 *
 * @param {string | URL} subpath
 * @param {URL} packageJSONUrl
 * @param {boolean} internal
 * @param {string | URL | undefined} base
 */
function throwInvalidSubpath(subpath, packageJSONUrl, internal, base) {
  const reason = `request is not a valid subpath for the "${internal ?
    'imports' : 'exports'}" resolution of ${fileURLToPath(packageJSONUrl)}`;
  throw new ERR_INVALID_MODULE_SPECIFIER(subpath, reason,
                                         base && fileURLToPath(base));
}

function throwInvalidPackageTarget(
  subpath, target, packageJSONUrl, internal, base) {
  if (typeof target === 'object' && target !== null) {
    target = JSONStringify(target, null, '');
  } else {
    target = `${target}`;
  }
  throw new ERR_INVALID_PACKAGE_TARGET(
    fileURLToPath(new URL('.', packageJSONUrl)), subpath, target,
    internal, base && fileURLToPath(base));
}

const invalidSegmentRegEx = /(^|\\|\/)((\.|%2e)(\.|%2e)?|(n|%6e|%4e)(o|%6f|%4f)(d|%64|%44)(e|%65|%45)(_|%5f)(m|%6d|%4d)(o|%6f|%4f)(d|%64|%44)(u|%75|%55)(l|%6c|%4c)(e|%65|%45)(s|%73|%53))(\\|\/|$)/i;
const invalidPackageNameRegEx = /^\.|%|\\/;
const patternRegEx = /\*/g;

function resolvePackageTargetString(
  target, subpath, match, packageJSONUrl, base, pattern, internal, conditions) {

  if (subpath !== '' && !pattern && target[target.length - 1] !== '/')
    throwInvalidPackageTarget(match, target, packageJSONUrl, internal, base);

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
    throwInvalidPackageTarget(match, target, packageJSONUrl, internal, base);
  }

  if (RegExpPrototypeTest(invalidSegmentRegEx, StringPrototypeSlice(target, 2)))
    throwInvalidPackageTarget(match, target, packageJSONUrl, internal, base);

  const resolved = new URL(target, packageJSONUrl);
  const resolvedPath = resolved.pathname;
  const packagePath = new URL('.', packageJSONUrl).pathname;

  if (!StringPrototypeStartsWith(resolvedPath, packagePath))
    throwInvalidPackageTarget(match, target, packageJSONUrl, internal, base);

  if (subpath === '') return resolved;

  if (RegExpPrototypeTest(invalidSegmentRegEx, subpath))
    throwInvalidSubpath(match + subpath, packageJSONUrl, internal, base);

  if (pattern) {
    return new URL(
      RegExpPrototypeSymbolReplace(
        patternRegEx,
        resolved.href,
        () => subpath
      )
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

function resolvePackageTarget(packageJSONUrl, target, subpath, packageSubpath,
                              base, pattern, internal, conditions) {
  if (typeof target === 'string') {
    return resolvePackageTargetString(
      target, subpath, packageSubpath, packageJSONUrl, base, pattern, internal,
      conditions);
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
          internal, conditions);
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
          pattern, internal, conditions);
        if (resolveResult === undefined)
          continue;
        return resolveResult;
      }
    }
    return undefined;
  } else if (target === null) {
    return null;
  }
  throwInvalidPackageTarget(packageSubpath, target, packageJSONUrl, internal,
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
    const resolved = resolvePackageTarget(
      packageJSONUrl, target, '', packageSubpath, base, false, false, conditions
    );

    if (resolved == null) {
      throwExportsNotFound(packageSubpath, packageJSONUrl, base);
    }

    return { resolved, exact: true };
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
      if (StringPrototypeEndsWith(packageSubpath, '/'))
        emitTrailingSlashPatternDeprecation(packageSubpath, packageJSONUrl,
                                            true, base);
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
    } else if (key[key.length - 1] === '/' &&
      StringPrototypeStartsWith(packageSubpath, key) &&
      patternKeyCompare(bestMatch, key) === 1) {
      bestMatch = key;
      bestMatchSubpath = StringPrototypeSlice(packageSubpath, key.length);
    }
  }

  if (bestMatch) {
    const target = exports[bestMatch];
    const pattern = StringPrototypeIncludes(bestMatch, '*');
    const resolved = resolvePackageTarget(
      packageJSONUrl,
      target,
      bestMatchSubpath,
      bestMatch,
      base,
      pattern,
      false,
      conditions);

    if (resolved == null) {
      throwExportsNotFound(packageSubpath, packageJSONUrl, base);
    }

    if (!pattern) {
      emitFolderMapDeprecation(bestMatch, packageJSONUrl, true, base);
    }

    return { resolved, exact: pattern };
  }

  throwExportsNotFound(packageSubpath, packageJSONUrl, base);
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
  if (name === '#' || StringPrototypeStartsWith(name, '#/')) {
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
          !StringPrototypeIncludes(name, '*') &&
          !StringPrototypeEndsWith(name, '/')) {
        const resolved = resolvePackageTarget(
          packageJSONUrl, imports[name], '', name, base, false, true, conditions
        );
        if (resolved != null) {
          return { resolved, exact: true };
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
          } else if (key[key.length - 1] === '/' &&
            StringPrototypeStartsWith(name, key) &&
            patternKeyCompare(bestMatch, key) === 1) {
            bestMatch = key;
            bestMatchSubpath = StringPrototypeSlice(name, key.length);
          }
        }

        if (bestMatch) {
          const target = imports[bestMatch];
          const pattern = StringPrototypeIncludes(bestMatch, '*');
          const resolved = resolvePackageTarget(
            packageJSONUrl, target,
            bestMatchSubpath, bestMatch,
            base, pattern, true,
            conditions);
          if (resolved !== null) {
            if (!pattern)
              emitFolderMapDeprecation(bestMatch, packageJSONUrl, false, base);
            return { resolved, exact: pattern };
          }
        }
      }
    }
  }
  throwImportNotDefined(name, packageJSONUrl, base);
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
 * @returns {URL}
 */
function packageResolve(specifier, base, conditions) {
  if (NativeModule.canBeRequiredByUsers(specifier))
    return new URL('node:' + specifier);

  const { packageName, packageSubpath, isScoped } =
    parsePackageName(specifier, base);

  // ResolveSelf
  const packageConfig = getPackageScopeConfig(base);
  if (packageConfig.exists) {
    const packageJSONUrl = pathToFileURL(packageConfig.pjsonPath);
    if (packageConfig.name === packageName &&
        packageConfig.exports !== undefined && packageConfig.exports !== null) {
      return packageExportsResolve(
        packageJSONUrl, packageSubpath, packageConfig, base, conditions
      ).resolved;
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
        packageJSONUrl, packageSubpath, packageConfig, base, conditions
      ).resolved;
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
 * @param {string} specifier
 * @returns {boolean}
 */
function isBareSpecifier(specifier) {
  return specifier[0] && specifier[0] !== '/' && specifier[0] !== '.';
}

function isRelativeSpecifier(specifier) {
  if (specifier[0] === '.') {
    if (specifier.length === 1 || specifier[1] === '/') return true;
    if (specifier[1] === '.') {
      if (specifier.length === 2 || specifier[2] === '/') return true;
    }
  }
  return false;
}

function shouldBeTreatedAsRelativeOrAbsolutePath(specifier) {
  if (specifier === '') return false;
  if (specifier[0] === '/') return true;
  return isRelativeSpecifier(specifier);
}

/**
 * @param {string} specifier
 * @param {string | URL | undefined} base
 * @param {Set<string>} conditions
 * @param {boolean} preserveSymlinks
 * @returns {URL}
 */
function moduleResolve(specifier, base, conditions, preserveSymlinks) {
  const isRemote = base.protocol === 'http:' ||
    base.protocol === 'https:';
  // Order swapped from spec for minor perf gain.
  // Ok since relative URLs cannot parse as URLs.
  let resolved;
  if (shouldBeTreatedAsRelativeOrAbsolutePath(specifier)) {
    resolved = new URL(specifier, base);
  } else if (!isRemote && specifier[0] === '#') {
    ({ resolved } = packageImportsResolve(specifier, base, conditions));
  } else {
    try {
      resolved = new URL(specifier);
    } catch {
      if (!isRemote) {
        resolved = packageResolve(specifier, base, conditions);
      }
    }
  }
  if (resolved.protocol !== 'file:')
    return resolved;
  return finalizeResolution(resolved, base, preserveSymlinks);
}

/**
 * Try to resolve an import as a CommonJS module
 * @param {string} specifier
 * @param {string} parentURL
 * @returns {boolean|string}
 */
function resolveAsCommonJS(specifier, parentURL) {
  try {
    const parent = fileURLToPath(parentURL);
    const tmpModule = new CJSModule(parent, null);
    tmpModule.paths = CJSModule._nodeModulePaths(parent);

    let found = CJSModule._resolveFilename(specifier, tmpModule, false);

    // If it is a relative specifier return the relative path
    // to the parent
    if (isRelativeSpecifier(specifier)) {
      found = relative(parent, found);
      // Add '.separator if the path does not start with '..separator'
      // This should be a safe assumption because when loading
      // esm modules there should be always a file specified so
      // there should not be a specifier like '..' or '.'
      if (!StringPrototypeStartsWith(found, `..${sep}`)) {
        found = `.${sep}${found}`;
      }
    } else if (isBareSpecifier(specifier)) {
      // If it is a bare specifier return the relative path within the
      // module
      const pkg = StringPrototypeSplit(specifier, '/')[0];
      const index = StringPrototypeIndexOf(found, pkg);
      if (index !== -1) {
        found = StringPrototypeSlice(found, index);
      }
    }
    // Normalize the path separator to give a valid suggestion
    // on Windows
    if (process.platform === 'win32') {
      found = RegExpPrototypeSymbolReplace(new RegExp(`\\${sep}`, 'g'),
                                           found, '/');
    }
    return found;
  } catch {
    return false;
  }
}

// TODO(@JakobJingleheimer): de-dupe `specifier` & `parsed`
function checkIfDisallowedImport(specifier, parsed, parsedParentURL) {
  if (parsedParentURL) {
    const parentURL = fileURLToPath(parsedParentURL?.href);

    if (
      parsedParentURL.protocol === 'http:' ||
      parsedParentURL.protocol === 'https:'
    ) {
      if (shouldBeTreatedAsRelativeOrAbsolutePath(specifier)) {
        // data: and blob: disallowed due to allowing file: access via
        // indirection
        if (parsed &&
          parsed.protocol !== 'https:' &&
          parsed.protocol !== 'http:'
        ) {
          throw new ERR_NETWORK_IMPORT_DISALLOWED(
            specifier,
            parentURL,
            'remote imports cannot import from a local location.'
          );
        }

        return { url: parsed.href };
      }
      if (NativeModule.canBeRequiredByUsers(specifier)) {
        throw new ERR_NETWORK_IMPORT_DISALLOWED(
          specifier,
          parentURL,
          'remote imports cannot import from a local location.'
        );
      }

      throw new ERR_NETWORK_IMPORT_DISALLOWED(
        specifier,
        parentURL,
        'only relative and absolute specifiers are supported.'
      );
    }
  }
}

function throwIfUnsupportedURLProtocol(url) {
  if (url.protocol !== 'file:' && url.protocol !== 'data:' &&
      url.protocol !== 'node:') {
    throw new ERR_UNSUPPORTED_ESM_URL_SCHEME(url);
  }
}

function throwIfUnsupportedURLScheme(parsed, experimentalNetworkImports) {
  if (
    parsed &&
    parsed.protocol !== 'file:' &&
    parsed.protocol !== 'data:' &&
    (
      !experimentalNetworkImports ||
      (
        parsed.protocol !== 'https:' &&
        parsed.protocol !== 'http:'
      )
    )
  ) {
    throw new ERR_UNSUPPORTED_ESM_URL_SCHEME(parsed, ArrayPrototypeConcat(
      'file',
      'data',
      experimentalNetworkImports ? ['https', 'http'] : [],
    ));
  }
}

async function defaultResolve(specifier, context = {}, defaultResolveUnused) {
  let { parentURL, conditions } = context;
  if (parentURL && policy?.manifest) {
    const redirects = policy.manifest.getDependencyMapper(parentURL);
    if (redirects) {
      const { resolve, reaction } = redirects;
      const destination = resolve(specifier, new SafeSet(conditions));
      let missing = true;
      if (destination === true) {
        missing = false;
      } else if (destination) {
        const href = destination.href;
        return { url: href };
      }
      if (missing) {
        // Prevent network requests from firing if resolution would be banned.
        // Network requests can extract data by doing things like putting
        // secrets in query params
        reaction(new ERR_MANIFEST_DEPENDENCY_MISSING(
          parentURL,
          specifier,
          ArrayPrototypeJoin([...conditions], ', '))
        );
      }
    }
  }

  let parsedParentURL;
  if (parentURL) {
    try {
      parsedParentURL = new URL(parentURL);
    } catch {
      // Ignore exception
    }
  }

  let parsed;
  try {
    if (shouldBeTreatedAsRelativeOrAbsolutePath(specifier)) {
      parsed = new URL(specifier, parsedParentURL);
    } else {
      parsed = new URL(specifier);
    }

    if (parsed.protocol === 'data:' ||
      (experimentalNetworkImports &&
        (
          parsed.protocol === 'https:' ||
          parsed.protocol === 'http:'
        )
      )
    ) {
      return { url: specifier };
    }
  } catch {
    // Ignore exception
  }

  // There are multiple deep branches that can either throw or return; instead
  // of duplicating that deeply nested logic for the possible returns, DRY and
  // check for a return. This seems the least gnarly.
  const maybeReturn = checkIfDisallowedImport(
    specifier,
    parsed,
    parsedParentURL,
  );

  if (maybeReturn) return maybeReturn;

  // This must come after checkIfDisallowedImport
  if (parsed && parsed.protocol === 'node:') return { url: specifier };

  throwIfUnsupportedURLScheme(parsed, experimentalNetworkImports);

  const isMain = parentURL === undefined;
  if (isMain) {
    parentURL = pathToFileURL(`${process.cwd()}/`).href;

    // This is the initial entry point to the program, and --input-type has
    // been passed as an option; but --input-type can only be used with
    // --eval, --print or STDIN string input. It is not allowed with file
    // input, to avoid user confusion over how expansive the effect of the
    // flag should be (i.e. entry point only, package scope surrounding the
    // entry point, etc.).
    if (typeFlag) throw new ERR_INPUT_TYPE_NOT_ALLOWED();
  }

  conditions = getConditionsSet(conditions);
  let url;
  try {
    url = moduleResolve(specifier, parentURL, conditions,
                        isMain ? preserveSymlinksMain : preserveSymlinks);
  } catch (error) {
    // Try to give the user a hint of what would have been the
    // resolved CommonJS module
    if (error.code === 'ERR_MODULE_NOT_FOUND' ||
        error.code === 'ERR_UNSUPPORTED_DIR_IMPORT') {
      if (StringPrototypeStartsWith(specifier, 'file://')) {
        specifier = fileURLToPath(specifier);
      }
      const found = resolveAsCommonJS(specifier, parentURL);
      if (found) {
        // Modify the stack and message string to include the hint
        const lines = StringPrototypeSplit(error.stack, '\n');
        const hint = `Did you mean to import ${found}?`;
        error.stack =
          ArrayPrototypeShift(lines) + '\n' +
          hint + '\n' +
          ArrayPrototypeJoin(lines, '\n');
        error.message += `\n${hint}`;
      }
    }
    throw error;
  }

  throwIfUnsupportedURLProtocol(url);

  return {
    // Do NOT cast `url` to a string: that will work even when there are real
    // problems, silencing them
    url: url.href,
    format: defaultGetFormatWithoutErrors(url, context),
  };
}

module.exports = {
  DEFAULT_CONDITIONS,
  defaultResolve,
  encodedSepRegEx,
  getPackageScopeConfig,
  getPackageType,
  packageExportsResolve,
  packageImportsResolve,
};

// cycle
const {
  defaultGetFormatWithoutErrors,
} = require('internal/modules/esm/get_format');

if (policy) {
  const $defaultResolve = defaultResolve;
  module.exports.defaultResolve = async function defaultResolve(
    specifier,
    context
  ) {
    const ret = await $defaultResolve(specifier, context, $defaultResolve);
    // This is a preflight check to avoid data exfiltration by query params etc.
    policy.manifest.mightAllow(ret.url, () =>
      new ERR_MANIFEST_DEPENDENCY_MISSING(
        context.parentURL,
        specifier,
        context.conditions
      )
    );
    return ret;
  };
}
