'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeJoin,
  ArrayPrototypeShift,
  JSONParse,
  JSONStringify,
  ObjectFreeze,
  ObjectGetOwnPropertyNames,
  ObjectPrototypeHasOwnProperty,
  RegExp,
  RegExpPrototypeTest,
  SafeMap,
  SafeSet,
  String,
  StringPrototypeEndsWith,
  StringPrototypeIncludes,
  StringPrototypeIndexOf,
  StringPrototypeReplace,
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
  StringPrototypeSubstr,
} = primordials;
const internalFS = require('internal/fs/utils');
const { NativeModule } = require('internal/bootstrap/loaders');
const {
  realpathSync,
  statSync,
  Stats,
} = require('fs');
const { getOptionValue } = require('internal/options');
const { sep, relative } = require('path');
const preserveSymlinks = getOptionValue('--preserve-symlinks');
const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
const typeFlag = getOptionValue('--input-type');
const { URL, pathToFileURL, fileURLToPath } = require('internal/url');
const {
  ERR_INPUT_TYPE_NOT_ALLOWED,
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_MODULE_SPECIFIER,
  ERR_INVALID_PACKAGE_CONFIG,
  ERR_INVALID_PACKAGE_TARGET,
  ERR_MODULE_NOT_FOUND,
  ERR_PACKAGE_IMPORT_NOT_DEFINED,
  ERR_PACKAGE_PATH_NOT_EXPORTED,
  ERR_UNSUPPORTED_DIR_IMPORT,
  ERR_UNSUPPORTED_ESM_URL_SCHEME,
} = require('internal/errors').codes;
const { Module: CJSModule } = require('internal/modules/cjs/loader');

const packageJsonReader = require('internal/modules/package_json_reader');
const DEFAULT_CONDITIONS = ObjectFreeze(['node', 'import']);
const DEFAULT_CONDITIONS_SET = new SafeSet(DEFAULT_CONDITIONS);


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
const packageJSONCache = new SafeMap();  /* string -> PackageConfig */

function tryStatSync(path) {
  try {
    return statSync(path);
  } catch {
    return new Stats();
  }
}

/**
 *
 * '/foo/package.json' -> '/foo'
 */
function removePackageJsonFromPath(path) {
  return StringPrototypeSlice(path, 0, path.length - 13);
}

function getPackageConfig(path) {
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
    const errorPath = removePackageJsonFromPath(path);
    throw new ERR_INVALID_PACKAGE_CONFIG(errorPath, error.message, true);
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

function getPackageScopeConfig(resolved, base) {
  let packageJSONUrl = new URL('./package.json', resolved);
  while (true) {
    const packageJSONPath = packageJSONUrl.pathname;
    if (StringPrototypeEndsWith(packageJSONPath, 'node_modules/package.json'))
      break;
    const packageConfig = getPackageConfig(fileURLToPath(packageJSONUrl), base);
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

/*
 * Legacy CommonJS main resolution:
 * 1. let M = pkg_url + (json main field)
 * 2. TRY(M, M.js, M.json, M.node)
 * 3. TRY(M/index.js, M/index.json, M/index.node)
 * 4. TRY(pkg_url/index.js, pkg_url/index.json, pkg_url/index.node)
 * 5. NOT_FOUND
 */
function fileExists(url) {
  return tryStatSync(fileURLToPath(url)).isFile();
}

function legacyMainResolve(packageJSONUrl, packageConfig) {
  let guess;
  if (packageConfig.main !== undefined) {
    // Note: fs check redundances will be handled by Descriptor cache here.
    if (fileExists(guess = new URL(`./${packageConfig.main}`,
                                   packageJSONUrl))) {
      return guess;
    }
    if (fileExists(guess = new URL(`./${packageConfig.main}.js`,
                                   packageJSONUrl))) {
      return guess;
    }
    if (fileExists(guess = new URL(`./${packageConfig.main}.json`,
                                   packageJSONUrl))) {
      return guess;
    }
    if (fileExists(guess = new URL(`./${packageConfig.main}.node`,
                                   packageJSONUrl))) {
      return guess;
    }
    if (fileExists(guess = new URL(`./${packageConfig.main}/index.js`,
                                   packageJSONUrl))) {
      return guess;
    }
    if (fileExists(guess = new URL(`./${packageConfig.main}/index.json`,
                                   packageJSONUrl))) {
      return guess;
    }
    if (fileExists(guess = new URL(`./${packageConfig.main}/index.node`,
                                   packageJSONUrl))) {
      return guess;
    }
    // Fallthrough.
  }
  if (fileExists(guess = new URL('./index.js', packageJSONUrl))) {
    return guess;
  }
  // So fs.
  if (fileExists(guess = new URL('./index.json', packageJSONUrl))) {
    return guess;
  }
  if (fileExists(guess = new URL('./index.node', packageJSONUrl))) {
    return guess;
  }
  // Not found.
  return undefined;
}

function resolveExtensionsWithTryExactName(search) {
  if (fileExists(search)) return search;
  return resolveExtensions(search);
}

const extensions = ['.js', '.json', '.node', '.mjs'];
function resolveExtensions(search) {
  for (let i = 0; i < extensions.length; i++) {
    const extension = extensions[i];
    const guess = new URL(`${search.pathname}${extension}`, search);
    if (fileExists(guess)) return guess;
  }
  return undefined;
}

function resolveIndex(search) {
  return resolveExtensions(new URL('index', search));
}

const encodedSepRegEx = /%2F|%2C/i;
function finalizeResolution(resolved, base) {
  if (getOptionValue('--experimental-specifier-resolution') === 'node') {
    let file = resolveExtensionsWithTryExactName(resolved);
    if (file !== undefined) return file;
    if (!StringPrototypeEndsWith(resolved.pathname, '/')) {
      file = resolveIndex(new URL(`${resolved.pathname}/`, base));
    } else {
      file = resolveIndex(resolved);
    }
    if (file !== undefined) return file;
    throw new ERR_MODULE_NOT_FOUND(
      resolved.pathname, fileURLToPath(base), 'module');
  }

  if (RegExpPrototypeTest(encodedSepRegEx, resolved.pathname))
    throw new ERR_INVALID_MODULE_SPECIFIER(
      resolved.pathname, 'must not include encoded "/" or "\\" characters',
      fileURLToPath(base));

  const path = fileURLToPath(resolved);
  const stats = tryStatSync(path);

  if (stats.isDirectory()) {
    const err = new ERR_UNSUPPORTED_DIR_IMPORT(
      path || resolved.pathname, fileURLToPath(base));
    err.url = String(resolved);
    throw err;
  } else if (!stats.isFile()) {
    throw new ERR_MODULE_NOT_FOUND(
      path || resolved.pathname, fileURLToPath(base), 'module');
  }

  return resolved;
}

function throwImportNotDefined(specifier, packageJSONUrl, base) {
  throw new ERR_PACKAGE_IMPORT_NOT_DEFINED(
    specifier, packageJSONUrl && fileURLToPath(new URL('.', packageJSONUrl)),
    fileURLToPath(base));
}

function throwExportsNotFound(subpath, packageJSONUrl, base) {
  throw new ERR_PACKAGE_PATH_NOT_EXPORTED(
    fileURLToPath(new URL('.', packageJSONUrl)), subpath, fileURLToPath(base));
}

function throwInvalidSubpath(subpath, packageJSONUrl, internal, base) {
  const reason = `request is not a valid subpath for the "${internal ?
    'imports' : 'exports'}" resolution of ${fileURLToPath(packageJSONUrl)}${
    base ? ` imported from ${base}` : ''}`;
  throw new ERR_INVALID_MODULE_SPECIFIER(subpath, reason, fileURLToPath(base));
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
    internal, fileURLToPath(base));
}

function resolvePackageTargetString(
  target, subpath, match, packageJSONUrl, base, internal, conditions) {
  if (subpath !== '' && target[target.length - 1] !== '/')
    throwInvalidPackageTarget(match, target, packageJSONUrl, internal, base);

  if (!target.startsWith('./')) {
    if (internal && !target.startsWith('../') && !target.startsWith('/')) {
      let isURL = false;
      try {
        new URL(target);
        isURL = true;
      } catch {}
      if (!isURL)
        return packageResolve(target + subpath, base, conditions);
    }
    throwInvalidPackageTarget(match, target, packageJSONUrl, internal, base);
  }

  const resolved = new URL(target, packageJSONUrl);
  const resolvedPath = resolved.pathname;
  const packagePath = new URL('.', packageJSONUrl).pathname;

  if (!StringPrototypeStartsWith(resolvedPath, packagePath) ||
      StringPrototypeIncludes(
        resolvedPath, '/node_modules/', packagePath.length - 1))
    throwInvalidPackageTarget(match, target, packageJSONUrl, internal, base);

  if (subpath === '') return resolved;
  const subpathResolved = new URL(subpath, resolved);
  const subpathResolvedPath = subpathResolved.pathname;
  if (!StringPrototypeStartsWith(subpathResolvedPath, resolvedPath) ||
      StringPrototypeIncludes(subpathResolvedPath,
                              '/node_modules/', packagePath.length - 1))
    throwInvalidSubpath(match + subpath, packageJSONUrl, internal, base);
  return subpathResolved;
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

function resolvePackageTarget(
  packageJSONUrl, target, subpath, packageSubpath, base, internal, conditions) {
  if (typeof target === 'string') {
    const resolved = resolvePackageTargetString(
      target, subpath, packageSubpath, packageJSONUrl, base, internal,
      conditions);
    if (resolved === null)
      return null;
    return finalizeResolution(resolved, base);
  } else if (ArrayIsArray(target)) {
    if (target.length === 0)
      return null;

    let lastException;
    for (let i = 0; i < target.length; i++) {
      const targetItem = target[i];
      let resolved;
      try {
        resolved = resolvePackageTarget(
          packageJSONUrl, targetItem, subpath, packageSubpath, base, internal,
          conditions);
      } catch (e) {
        lastException = e;
        if (e.code === 'ERR_INVALID_PACKAGE_TARGET')
          continue;
        throw e;
      }
      if (resolved === undefined)
        continue;
      if (resolved === null) {
        lastException = null;
        continue;
      }
      return finalizeResolution(resolved, base);
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
          fileURLToPath(packageJSONUrl),
          '"exports" cannot contain numeric property keys');
      }
    }
    for (let i = 0; i < keys.length; i++) {
      const key = keys[i];
      if (key === 'default' || conditions.has(key)) {
        const conditionalTarget = target[key];
        const resolved = resolvePackageTarget(
          packageJSONUrl, conditionalTarget, subpath, packageSubpath, base,
          internal, conditions);
        if (resolved === undefined)
          continue;
        return resolved;
      }
    }
    return undefined;
  } else if (target === null) {
    return null;
  }
  throwInvalidPackageTarget(packageSubpath, target, packageJSONUrl, internal,
                            base);
}

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
        fileURLToPath(packageJSONUrl),
        '"exports" cannot contain some keys starting with \'.\' and some not.' +
        ' The exports object must either be an object of package subpath keys' +
        ' or an object of main entry condition name keys only.');
    }
  }
  return isConditionalSugar;
}

function packageMainResolve(packageJSONUrl, packageConfig, base, conditions) {
  if (packageConfig.exists) {
    const exports = packageConfig.exports;
    if (exports !== undefined) {
      if (isConditionalExportsMainSugar(exports, packageJSONUrl, base)) {
        const resolved = resolvePackageTarget(packageJSONUrl, exports, '', '',
                                              base, false, conditions);
        if (resolved === null || resolved === undefined)
          throwExportsNotFound('.', packageJSONUrl, base);
        return resolved;
      } else if (typeof exports === 'object' && exports !== null) {
        const target = exports['.'];
        if (target !== undefined) {
          const resolved = resolvePackageTarget(packageJSONUrl, target, '', '',
                                                base, false, conditions);
          if (resolved === null || resolved === undefined)
            throwExportsNotFound('.', packageJSONUrl, base);
          return resolved;
        }
      }

      throw new ERR_PACKAGE_PATH_NOT_EXPORTED(packageJSONUrl, '.');
    }
    if (getOptionValue('--experimental-specifier-resolution') === 'node') {
      if (packageConfig.main !== undefined) {
        return finalizeResolution(
          new URL(packageConfig.main, packageJSONUrl), base);
      }
      return finalizeResolution(
        new URL('index', packageJSONUrl), base);
    }
    return legacyMainResolve(packageJSONUrl, packageConfig);
  }

  throw new ERR_MODULE_NOT_FOUND(
    fileURLToPath(new URL('.', packageJSONUrl)), fileURLToPath(base));
}

/**
 * @param {URL} packageJSONUrl
 * @param {string} packageSubpath
 * @param {object} packageConfig
 * @param {string} base
 * @param {Set<string>} conditions
 * @returns {URL}
 */
function packageExportsResolve(
  packageJSONUrl, packageSubpath, packageConfig, base, conditions) {
  const exports = packageConfig.exports;
  if (exports === undefined ||
      isConditionalExportsMainSugar(exports, packageJSONUrl, base)) {
    throwExportsNotFound(packageSubpath, packageJSONUrl, base);
  }

  if (ObjectPrototypeHasOwnProperty(exports, packageSubpath)) {
    const target = exports[packageSubpath];
    const resolved = resolvePackageTarget(
      packageJSONUrl, target, '', packageSubpath, base, false, conditions);
    if (resolved === null || resolved === undefined)
      throwExportsNotFound(packageSubpath, packageJSONUrl, base);
    return finalizeResolution(resolved, base);
  }

  let bestMatch = '';
  const keys = ObjectGetOwnPropertyNames(exports);
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    if (key[key.length - 1] !== '/') continue;
    if (StringPrototypeStartsWith(packageSubpath, key) &&
        key.length > bestMatch.length) {
      bestMatch = key;
    }
  }

  if (bestMatch) {
    const target = exports[bestMatch];
    const subpath = StringPrototypeSubstr(packageSubpath, bestMatch.length);
    const resolved = resolvePackageTarget(
      packageJSONUrl, target, subpath, bestMatch, base, false, conditions);
    if (resolved === null || resolved === undefined)
      throwExportsNotFound(packageSubpath, packageJSONUrl, base);
    return finalizeResolution(resolved, base);
  }

  throwExportsNotFound(packageSubpath, packageJSONUrl, base);
}

function packageInternalResolve(name, base, conditions) {
  if (name === '#' || name.startsWith('#/')) {
    const reason = 'is not a valid internal imports specifier name';
    throw new ERR_INVALID_MODULE_SPECIFIER(name, reason, fileURLToPath(base));
  }
  let packageJSONUrl;
  const packageConfig = getPackageScopeConfig(base, base);
  if (packageConfig.exists) {
    packageJSONUrl = pathToFileURL(packageConfig.pjsonPath);
    const imports = packageConfig.imports;
    if (imports) {
      if (ObjectPrototypeHasOwnProperty(imports, name)) {
        const resolved = resolvePackageTarget(
          packageJSONUrl, imports[name], '', name, base, true, conditions);
        if (resolved !== null)
          return finalizeResolution(resolved, base);
      } else {
        let bestMatch = '';
        const keys = ObjectGetOwnPropertyNames(imports);
        for (let i = 0; i < keys.length; i++) {
          const key = keys[i];
          if (key[key.length - 1] !== '/') continue;
          if (StringPrototypeStartsWith(name, key) &&
              key.length > bestMatch.length) {
            bestMatch = key;
          }
        }

        if (bestMatch) {
          const target = imports[bestMatch];
          const subpath = StringPrototypeSubstr(name, bestMatch.length);
          const resolved = resolvePackageTarget(
            packageJSONUrl, target, subpath, bestMatch, base, true,
            conditions);
          if (resolved !== null)
            return finalizeResolution(resolved, base);
        }
      }
    }
  }
  throwImportNotDefined(name, packageJSONUrl, base);
}

function getPackageType(url) {
  const packageConfig = getPackageScopeConfig(url, url);
  return packageConfig.type;
}

/**
 * @param {string} specifier
 * @param {URL} base
 * @param {Set<string>} conditions
 * @returns {URL}
 */
function packageResolve(specifier, base, conditions) {
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
  // separators.
  for (let i = 0; i < packageName.length; i++) {
    if (packageName[i] === '%' || packageName[i] === '\\') {
      validPackageName = false;
      break;
    }
  }

  if (!validPackageName) {
    throw new ERR_INVALID_MODULE_SPECIFIER(
      specifier, 'is not a valid package name', fileURLToPath(base));
  }

  const packageSubpath = separatorIndex === -1 ?
    '' : '.' + StringPrototypeSlice(specifier, separatorIndex);

  // ResolveSelf
  const packageConfig = getPackageScopeConfig(base, base);
  if (packageConfig.exists) {
    const packageJSONUrl = pathToFileURL(packageConfig.pjsonPath);
    if (packageConfig.name === packageName &&
        packageConfig.exports !== undefined) {
      if (packageSubpath === './') {
        return new URL('./', packageJSONUrl);
      } else if (packageSubpath === '') {
        return packageMainResolve(packageJSONUrl, packageConfig, base,
                                  conditions);
      }
      return packageExportsResolve(
        packageJSONUrl, packageSubpath, packageConfig, base, conditions);
    }
  }

  let packageJSONUrl =
    new URL('./node_modules/' + packageName + '/package.json', base);
  let packageJSONPath = fileURLToPath(packageJSONUrl);
  let lastPath;
  do {
    const stat = tryStatSync(removePackageJsonFromPath(packageJSONPath));
    if (!stat.isDirectory()) {
      lastPath = packageJSONPath;
      packageJSONUrl = new URL((isScoped ?
        '../../../../node_modules/' : '../../../node_modules/') +
        packageName + '/package.json', packageJSONUrl);
      packageJSONPath = fileURLToPath(packageJSONUrl);
      continue;
    }

    // Package match.
    const packageConfig = getPackageConfig(packageJSONPath, base);
    if (packageSubpath === './') {
      return new URL('./', packageJSONUrl);
    } else if (packageSubpath === '') {
      return packageMainResolve(packageJSONUrl, packageConfig, base,
                                conditions);
    } else if (packageConfig.exports !== undefined) {
      return packageExportsResolve(
        packageJSONUrl, packageSubpath, packageConfig, base, conditions);
    }
    return finalizeResolution(
      new URL(packageSubpath, packageJSONUrl), base);
    // Cross-platform root check.
  } while (packageJSONPath.length !== lastPath.length);

  // eslint can't handle the above code.
  // eslint-disable-next-line no-unreachable
  throw new ERR_MODULE_NOT_FOUND(packageName, fileURLToPath(base));
}

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
 * @param {URL} base
 * @param {Set<string>} conditions
 * @returns {URL}
 */
function moduleResolve(specifier, base, conditions) {
  // Order swapped from spec for minor perf gain.
  // Ok since relative URLs cannot parse as URLs.
  let resolved;
  if (shouldBeTreatedAsRelativeOrAbsolutePath(specifier)) {
    resolved = new URL(specifier, base);
  } else if (specifier[0] === '#') {
    resolved = packageInternalResolve(specifier, base, conditions);
  } else {
    try {
      resolved = new URL(specifier);
    } catch {
      return packageResolve(specifier, base, conditions);
    }
  }
  return finalizeResolution(resolved, base);
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
      found = StringPrototypeReplace(found, new RegExp(`\\${sep}`, 'g'), '/');
    }
    return found;
  } catch {
    return false;
  }
}

function defaultResolve(specifier, context = {}, defaultResolveUnused) {
  let { parentURL, conditions } = context;
  let parsed;
  try {
    parsed = new URL(specifier);
    if (parsed.protocol === 'data:') {
      return {
        url: specifier
      };
    }
  } catch {}
  if (parsed && parsed.protocol === 'nodejs:')
    return { url: specifier };
  if (parsed && parsed.protocol !== 'file:' && parsed.protocol !== 'data:')
    throw new ERR_UNSUPPORTED_ESM_URL_SCHEME();
  if (NativeModule.canBeRequiredByUsers(specifier)) {
    return {
      url: 'nodejs:' + specifier
    };
  }
  if (parentURL && StringPrototypeStartsWith(parentURL, 'data:')) {
    // This is gonna blow up, we want the error
    new URL(specifier, parentURL);
  }

  const isMain = parentURL === undefined;
  if (isMain) {
    parentURL = pathToFileURL(`${process.cwd()}/`).href;

    // This is the initial entry point to the program, and --input-type has
    // been passed as an option; but --input-type can only be used with
    // --eval, --print or STDIN string input. It is not allowed with file
    // input, to avoid user confusion over how expansive the effect of the
    // flag should be (i.e. entry point only, package scope surrounding the
    // entry point, etc.).
    if (typeFlag)
      throw new ERR_INPUT_TYPE_NOT_ALLOWED();
  }

  conditions = getConditionsSet(conditions);
  let url;
  try {
    url = moduleResolve(specifier, parentURL, conditions);
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

  if (isMain ? !preserveSymlinksMain : !preserveSymlinks) {
    const urlPath = fileURLToPath(url);
    const real = realpathSync(urlPath, {
      [internalFS.realpathCacheKey]: realpathCache
    });
    const old = url;
    url = pathToFileURL(real + (urlPath.endsWith(sep) ? '/' : ''));
    url.search = old.search;
    url.hash = old.hash;
  }

  return { url: `${url}` };
}

module.exports = {
  DEFAULT_CONDITIONS,
  defaultResolve,
  encodedSepRegEx,
  getPackageType,
  packageInternalResolve
};
