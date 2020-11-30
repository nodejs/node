'use strict';
const {
  RegExpPrototypeExec,
  ObjectAssign,
  ObjectCreate,
  ObjectPrototypeHasOwnProperty,
  PromisePrototypeThen,
  PromiseResolve,
} = primordials;
const { extname } = require('path');
const { getOptionValue } = require('internal/options');
const { fetchModule } = require('internal/modules/esm/fetch_module');
const {
  extensionFormatMap,
  getLegacyExtensionFormat,
  mimeToFormat,
} = require('internal/modules/esm/formats');

const experimentalNetworkImports =
  getOptionValue('--experimental-network-imports');
const experimentalSpecifierResolution =
  getOptionValue('--experimental-specifier-resolution');
const { getPackageType } = require('internal/modules/esm/resolve');
const { URL, fileURLToPath } = require('internal/url');
const { ERR_UNKNOWN_FILE_EXTENSION } = require('internal/errors').codes;

const protocolHandlers = ObjectAssign(ObjectCreate(null), {
  'data:': getDataProtocolModuleFormat,
  'file:': getFileProtocolModuleFormat,
  'http:': getHttpProtocolModuleFormat,
  'https:': getHttpProtocolModuleFormat,
  'node:'() { return 'builtin'; },
});

function getDataProtocolModuleFormat(parsed) {
  const { 1: mime } = RegExpPrototypeExec(
    /^([^/]+\/[^;,]+)(?:[^,]*?)(;base64)?,/,
    parsed.pathname,
  ) || [ null, null, null ];

  return mimeToFormat(mime);
}

function getFileProtocolModuleFormat(url, context, ignoreErrors) {
  const ext = extname(url.pathname);
  if (ext === '.js') {
    return getPackageType(url) === 'module' ? 'module' : 'commonjs';
  }

  const format = extensionFormatMap[ext];
  if (format) return format;

  if (experimentalSpecifierResolution !== 'node') {
    // Explicit undefined return indicates load hook should rerun format check
    if (ignoreErrors) return undefined;
    throw new ERR_UNKNOWN_FILE_EXTENSION(ext, fileURLToPath(url));
  }

  return getLegacyExtensionFormat(ext) ?? null;
}

function getHttpProtocolModuleFormat(url, context) {
  if (experimentalNetworkImports) {
    return PromisePrototypeThen(
      PromiseResolve(fetchModule(url, context)),
      (entry) => {
        return mimeToFormat(entry.headers['content-type']);
      }
    );
  }
}

function defaultGetFormatWithoutErrors(url, context) {
  const parsed = new URL(url);
  if (!ObjectPrototypeHasOwnProperty(protocolHandlers, parsed.protocol))
    return null;
  return protocolHandlers[parsed.protocol](parsed, context, true);
}

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
};
