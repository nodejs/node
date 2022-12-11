'use strict';

const {
  ArrayPrototypeConcat,
  ArrayPrototypeJoin,
  ArrayPrototypeShift,
  RegExp,
  RegExpPrototypeExec,
  RegExpPrototypeSymbolReplace,
  SafeMap,
  SafeSet,
  String,
  StringPrototypeEndsWith,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
} = primordials;
const internalFS = require('internal/fs/utils');
const { BuiltinModule } = require('internal/bootstrap/loaders');
const {
  realpathSync,
} = require('fs');
const { getOptionValue } = require('internal/options');
const { getLazy } = require('internal/util');
const policy = getLazy(
  () => (getOptionValue('--experimental-policy') ? require('internal/process/policy') : null)
);

const { sep, relative } = require('path');
const { URL, pathToFileURL, fileURLToPath } = require('internal/url');
const {
  ERR_INPUT_TYPE_NOT_ALLOWED,
  ERR_INVALID_MODULE_SPECIFIER,
  ERR_MANIFEST_DEPENDENCY_MISSING,
  ERR_MODULE_NOT_FOUND,
  ERR_UNSUPPORTED_DIR_IMPORT,
  ERR_NETWORK_IMPORT_DISALLOWED,
  ERR_UNSUPPORTED_ESM_URL_SCHEME,
} = require('internal/errors').codes;

const {
  getConditionsSet,
  packageResolve,
  packageImportsResolve,
  encodedSepRegEx,
  tryStatSync,
  defaultGetFormatWithoutErrors,
} = require('internal/modules/esm/utils');


/**
 * @typedef {import('internal/modules/esm/utils.js').PackageConfig} PackageConfig
 */

const realpathCache = new SafeMap();

/**
 * @param {URL} resolved
 * @param {string | URL | undefined} base
 * @param {boolean} preserveSymlinks
 * @returns {URL | undefined}
 */
function finalizeResolution(resolved, base, preserveSymlinks) {
  if (RegExpPrototypeExec(encodedSepRegEx, resolved.pathname) !== null)
    throw new ERR_INVALID_MODULE_SPECIFIER(
      resolved.pathname, 'must not include encoded "/" or "\\" characters',
      fileURLToPath(base));

  const path = fileURLToPath(resolved);

  const stats = tryStatSync(StringPrototypeEndsWith(path, '/') ?
    StringPrototypeSlice(path, -1) : path);
  if (stats.isDirectory()) {
    const err = new ERR_UNSUPPORTED_DIR_IMPORT(path, fileURLToPath(base));
    err.url = String(resolved);
    throw err;
  } else if (!stats.isFile()) {
    if (process.env.WATCH_REPORT_DEPENDENCIES && process.send) {
      process.send({ 'watch:require': [path || resolved.pathname] });
    }
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
 * @returns {url: URL, format?: string}
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
    resolved = packageImportsResolve(specifier, base, conditions);
  } else {
    try {
      resolved = new URL(specifier);
    } catch {
      if (!isRemote) {
        resolved = packageResolve(specifier, base, conditions);
      }
    }
  }
  if (resolved.protocol !== 'file:') {
    return resolved;
  }
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
    const { Module: CJSModule } = require('internal/modules/cjs/loader');
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
            parsedParentURL,
            'remote imports cannot import from a local location.'
          );
        }

        return { url: parsed.href };
      }
      if (BuiltinModule.canBeRequiredByUsers(specifier) &&
          BuiltinModule.canBeRequiredWithoutScheme(specifier)) {
        throw new ERR_NETWORK_IMPORT_DISALLOWED(
          specifier,
          parsedParentURL,
          'remote imports cannot import from a local location.'
        );
      }

      throw new ERR_NETWORK_IMPORT_DISALLOWED(
        specifier,
        parsedParentURL,
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

async function defaultResolveWithoutPolicy(specifier, context = {}) {
  let { parentURL, conditions } = context;
  if (parentURL && policy()?.manifest) {
    const redirects = policy().manifest.getDependencyMapper(parentURL);
    if (redirects) {
      const { resolve, reaction } = redirects;
      const destination = resolve(specifier, new SafeSet(conditions));
      let missing = true;
      if (destination === true) {
        missing = false;
      } else if (destination) {
        const href = destination.href;
        return { __proto__: null, url: href };
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
      (getOptionValue('--experimental-network-imports') &&
        (
          parsed.protocol === 'https:' ||
          parsed.protocol === 'http:'
        )
      )
    ) {
      return { __proto__: null, url: parsed.href };
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
  if (parsed && parsed.protocol === 'node:') return { __proto__: null, url: specifier };

  throwIfUnsupportedURLScheme(parsed, getOptionValue('--experimental-network-imports'));

  const isMain = parentURL === undefined;
  if (isMain) {
    parentURL = pathToFileURL(`${process.cwd()}/`).href;

    // This is the initial entry point to the program, and --input-type has
    // been passed as an option; but --input-type can only be used with
    // --eval, --print or STDIN string input. It is not allowed with file
    // input, to avoid user confusion over how expansive the effect of the
    // flag should be (i.e. entry point only, package scope surrounding the
    // entry point, etc.).
    if (getOptionValue('--input-type')) throw new ERR_INPUT_TYPE_NOT_ALLOWED();
  }

  conditions = getConditionsSet(conditions);
  let url;
  try {
    url = moduleResolve(
      specifier,
      parentURL,
      conditions,
      isMain ? getOptionValue('--preserve-symlinks-main') : getOptionValue('--preserve-symlinks')
    );
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
    __proto__: null,
    // Do NOT cast `url` to a string: that will work even when there are real
    // problems, silencing them
    url: url.href,
    format: defaultGetFormatWithoutErrors(url, context),
  };
}

async function defaultResolveWithPolicy(specifier, context) {
  const ret = await defaultResolveWithoutPolicy(specifier, context);
  // This is a preflight check to avoid data exfiltration by query params etc.
  policy().manifest.mightAllow(ret.url, () =>
    new ERR_MANIFEST_DEPENDENCY_MISSING(
      context.parentURL,
      specifier,
      context.conditions
    )
  );
  return ret;
}

async function defaultResolve(specifier, context) {
  if (policy()) {
    return defaultResolveWithPolicy(specifier, context);
  }
  return defaultResolveWithoutPolicy(specifier, context);
}

module.exports = {
  defaultResolve,
};
