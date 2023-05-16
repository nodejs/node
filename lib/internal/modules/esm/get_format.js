'use strict';
const {
  RegExpPrototypeExec,
  ObjectPrototypeHasOwnProperty,
  PromisePrototypeThen,
  PromiseResolve,
  StringPrototypeCharCodeAt,
  StringPrototypeSlice,
} = primordials;
const { basename, relative } = require('path');
const { getOptionValue } = require('internal/options');
const {
  extensionFormatMap,
  mimeToFormat,
} = require('internal/modules/esm/formats');

const experimentalNetworkImports =
  getOptionValue('--experimental-network-imports');
const { getPackageType, getPackageScopeConfig } = require('internal/modules/esm/resolve');
const { URL, fileURLToPath } = require('internal/url');
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
 * @param {URL} url
 * @param {{parentURL: string}} context
 * @param {boolean} ignoreErrors
 * @returns {string}
 */
function getFileProtocolModuleFormat(url, context, ignoreErrors) {
  const ext = extname(url);
  if (ext === '.js') {
    return getPackageType(url) === 'module' ? 'module' : 'commonjs';
  }

  const format = extensionFormatMap[ext];
  if (format) return format;

  // Explicit undefined return indicates load hook should rerun format check
  if (ignoreErrors) { return undefined; }
  const filepath = fileURLToPath(url);
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

module.exports = {
  defaultGetFormat,
  defaultGetFormatWithoutErrors,
  extensionFormatMap,
  extname,
};
