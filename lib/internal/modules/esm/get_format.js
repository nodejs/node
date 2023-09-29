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
const { basename, relative } = require('path');
const { getOptionValue } = require('internal/options');
const {
  extensionFormatMap,
  getFormatOfExtensionlessFile,
  mimeToFormat,
} = require('internal/modules/esm/formats');

const experimentalNetworkImports =
  getOptionValue('--experimental-network-imports');
const defaultTypeFlag = getOptionValue('--experimental-default-type');
// The next line is where we flip the default to ES modules someday.
const defaultType = defaultTypeFlag === 'module' ? 'module' : 'commonjs';
const { getPackageType, getPackageScopeConfig } = require('internal/modules/esm/resolve');
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
 * @param {{parentURL: string}} context
 * @param {boolean} ignoreErrors
 * @returns {string}
 */
function getFileProtocolModuleFormat(url, context, ignoreErrors) {
  const ext = extname(url);

  if (ext === '.js') {
    const packageType = getPackageType(url);
    if (packageType !== 'none') {
      return packageType;
    }
    // The controlling `package.json` file has no `type` field.
    if (defaultType === 'module') {
      // An exception to the type flag making ESM the default everywhere is that package scopes under `node_modules`
      // should retain the assumption that a lack of a `type` field means CommonJS.
      return underNodeModules(url) ? 'commonjs' : 'module';
    }
    return 'commonjs';
  }

  if (ext === '') {
    const packageType = getPackageType(url);
    if (defaultType === 'commonjs') { // Legacy behavior
      if (packageType === 'none' || packageType === 'commonjs') {
        return 'commonjs';
      }
      // If package type is `module`, fall through to the error case below
    } else { // Else defaultType === 'module'
      if (underNodeModules(url)) { // Exception for package scopes under `node_modules`
        return 'commonjs';
      }
      if (packageType === 'none' || packageType === 'module') {
        return getFormatOfExtensionlessFile(url);
      } // Else packageType === 'commonjs'
      return 'commonjs';
    }
  }

  const format = extensionFormatMap[ext];
  if (format) { return format; }

  // Explicit undefined return indicates load hook should rerun format check
  if (ignoreErrors) { return undefined; }
  const filepath = fileURLToPath(url);
  let suggestion = '';
  if (getPackageType(url) === 'module' && ext === '') {
    const config = getPackageScopeConfig(url);
    const fileBasename = basename(filepath);
    const relativePath = StringPrototypeSlice(relative(config.pjsonPath, filepath), 1);
    suggestion = 'Loading extensionless files is not supported inside of "type":"module" package.json contexts ' +
      `without --experimental-default-type=module. The package.json file ${config.pjsonPath} caused this "type":"module" ` +
      `context. Try changing ${filepath} to have a file extension. Note the "bin" field of package.json can point ` +
      `to a file with an extension, for example {"type":"module","bin":{"${fileBasename}":"${relativePath}.js"}}`;
  }
  throw new ERR_UNKNOWN_FILE_EXTENSION(ext, filepath, suggestion);
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
