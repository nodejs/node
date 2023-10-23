'use strict';

const {
  RegExpPrototypeExec,
  ObjectPrototypeHasOwnProperty,
  PromisePrototypeThen,
  PromiseResolve,
  StringPrototypeIncludes,
  StringPrototypeCharCodeAt,
  StringPrototypeSlice,
} = primordials;
const { getOptionValue } = require('internal/options');
const {
  extensionFormatMap,
  getFormatOfExtensionlessFile,
  mimeToFormat,
} = require('internal/modules/esm/formats');

const experimentalNetworkImports =
  getOptionValue('--experimental-network-imports');
const { containsModuleSyntax } = internalBinding('contextify');
const { getPackageType } = require('internal/modules/esm/resolve');
const { fileURLToPath } = require('internal/url');
const { ERR_UNKNOWN_FILE_EXTENSION } = require('internal/errors').codes;

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

const DOT_CODE = 46;
const SLASH_CODE = 47;

/**
 * Returns the file extension from a URL. Should give similar result to
 * `require('node:path').extname(require('node:url').fileURLToPath(url))`
 * when used with a `file:` URL.
 * @param {URL} url
 * @returns {string}
 */
function extname(url) {
  const { pathname } = url;
  for (let i = pathname.length - 1; i > 0; i--) {
    switch (StringPrototypeCharCodeAt(pathname, i)) {
      case SLASH_CODE:
        return '';

      case DOT_CODE:
        return StringPrototypeCharCodeAt(pathname, i - 1) === SLASH_CODE ? '' : StringPrototypeSlice(pathname, i);
    }
  }
  return '';
}

/**
 * Determine whether the given file URL is under a `node_modules` folder.
 * This function assumes that the input has already been verified to be a `file:` URL,
 * and is a file rather than a folder.
 * @param {URL} url
 */
function underNodeModules(url) {
  if (url.protocol !== 'file:') { return false; } // We determine module types for other protocols based on MIME header

  return StringPrototypeIncludes(url.pathname, '/node_modules/');
}

/**
 * @param {URL} url
 * @param {{parentURL: string; source?: Buffer}} context
 * @param {boolean} ignoreErrors
 * @returns {string}
 */
function getFileProtocolModuleFormat(url, context = { __proto__: null }, ignoreErrors) {
  const { source } = context;
  const ext = extname(url);

  if (ext === '.js') {
    const packageType = getPackageType(url);
    if (packageType !== 'none') {
      return packageType;
    }

    // The controlling `package.json` file has no `type` field.
    switch (getOptionValue('--experimental-default-type')) {
      case 'module': { // The user explicitly passed `--experimental-default-type=module`.
        // An exception to the type flag making ESM the default everywhere is that package scopes under `node_modules`
        // should retain the assumption that a lack of a `type` field means CommonJS.
        return underNodeModules(url) ? 'commonjs' : 'module';
      }
      case 'commonjs': { // The user explicitly passed `--experimental-default-type=commonjs`.
        return 'commonjs';
      }
      default: { // The user did not pass `--experimental-default-type`.
        // `source` is undefined when this is called from `defaultResolve`;
        // but this gets called again from `defaultLoad`/`defaultLoadSync`.
        if (getOptionValue('--experimental-detect-module')) {
          return source ?
            (containsModuleSyntax(`${source}`, fileURLToPath(url)) ? 'module' : 'commonjs') :
            null;
        }
        return 'commonjs';
      }
    }
  }

  if (ext === '') {
    const packageType = getPackageType(url);
    if (packageType === 'module') {
      return getFormatOfExtensionlessFile(url);
    }
    if (packageType !== 'none') {
      return packageType; // 'commonjs' or future package types
    }

    // The controlling `package.json` file has no `type` field.
    switch (getOptionValue('--experimental-default-type')) {
      case 'module': { // The user explicitly passed `--experimental-default-type=module`.
        return underNodeModules(url) ? 'commonjs' : getFormatOfExtensionlessFile(url);
      }
      case 'commonjs': { // The user explicitly passed `--experimental-default-type=commonjs`.
        return 'commonjs';
      }
      default: { // The user did not pass `--experimental-default-type`.
        if (getOptionValue('--experimental-detect-module')) {
          if (!source) { return null; }
          const format = getFormatOfExtensionlessFile(url);
          if (format === 'module') {
            return containsModuleSyntax(`${source}`, fileURLToPath(url)) ? 'module' : 'commonjs';
          }
          return format;
        }
        return 'commonjs';
      }
    }
  }

  const format = extensionFormatMap[ext];
  if (format) { return format; }

  // Explicit undefined return indicates load hook should rerun format check
  if (ignoreErrors) { return undefined; }
  const filepath = fileURLToPath(url);
  throw new ERR_UNKNOWN_FILE_EXTENSION(ext, filepath);
}

/**
 * @param {URL} url
 * @param {{parentURL: string}} context
 * @returns {Promise<string> | undefined} only works when enabled
 */
function getHttpProtocolModuleFormat(url, context) {
  if (experimentalNetworkImports) {
    const { fetchModule } = require('internal/modules/esm/fetch_module');
    return PromisePrototypeThen(
      PromiseResolve(fetchModule(url, context)),
      (entry) => {
        return mimeToFormat(entry.headers['content-type']);
      },
    );
  }
}

/**
 * @param {URL} url
 * @param {{parentURL: string}} context
 * @returns {Promise<string> | string | undefined} only works when enabled
 */
function defaultGetFormatWithoutErrors(url, context) {
  const protocol = url.protocol;
  if (!ObjectPrototypeHasOwnProperty(protocolHandlers, protocol)) {
    return null;
  }
  return protocolHandlers[protocol](url, context, true);
}

/**
 * @param {URL} url
 * @param {{parentURL: string}} context
 * @returns {Promise<string> | string | undefined} only works when enabled
 */
function defaultGetFormat(url, context) {
  const protocol = url.protocol;
  if (!ObjectPrototypeHasOwnProperty(protocolHandlers, protocol)) {
    return null;
  }
  return protocolHandlers[protocol](url, context, false);
}

module.exports = {
  defaultGetFormat,
  defaultGetFormatWithoutErrors,
  extensionFormatMap,
  extname,
};
