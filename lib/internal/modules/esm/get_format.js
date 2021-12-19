'use strict';
const {
  ObjectAssign,
  ObjectCreate,
  ObjectPrototypeHasOwnProperty,
  RegExpPrototypeExec,
} = primordials;
const { extname } = require('path');
const { getOptionValue } = require('internal/options');

const experimentalJsonModules = getOptionValue('--experimental-json-modules');
const experimentalSpecifierResolution =
  getOptionValue('--experimental-specifier-resolution');
const experimentalWasmModules = getOptionValue('--experimental-wasm-modules');
const { getPackageType } = require('internal/modules/esm/resolve');
const { URL, fileURLToPath } = require('internal/url');
const { ERR_UNKNOWN_FILE_EXTENSION } = require('internal/errors').codes;

const extensionFormatMap = {
  '__proto__': null,
  '.cjs': 'commonjs',
  '.js': 'module',
  '.mjs': 'module'
};

const legacyExtensionFormatMap = {
  '__proto__': null,
  '.cjs': 'commonjs',
  '.js': 'commonjs',
  '.json': 'commonjs',
  '.mjs': 'module',
  '.node': 'commonjs'
};

let experimentalSpecifierResolutionWarned = false;

if (experimentalWasmModules)
  extensionFormatMap['.wasm'] = legacyExtensionFormatMap['.wasm'] = 'wasm';

if (experimentalJsonModules)
  extensionFormatMap['.json'] = legacyExtensionFormatMap['.json'] = 'json';

const protocolHandlers = ObjectAssign(ObjectCreate(null), {
  'data:'(parsed) {
    const { 1: mime } = RegExpPrototypeExec(
      /^([^/]+\/[^;,]+)(?:[^,]*?)(;base64)?,/,
      parsed.pathname,
    ) || [, null];
    const format = ({
      '__proto__': null,
      'text/javascript': 'module',
      'application/json': experimentalJsonModules ? 'json' : null,
      'application/wasm': experimentalWasmModules ? 'wasm' : null
    })[mime] || null;

    return format;
  },
  'file:'(parsed, url) {
    return getPackageFormat(parsed, true, url) || null;
  },
  'node:'() { return 'builtin'; },
});

function throwIfNotExperimentalSpecifierResolution(ext, url) {
  if (experimentalSpecifierResolution !== 'node') {
    throw new ERR_UNKNOWN_FILE_EXTENSION(ext, fileURLToPath(url));
  }
  return getLegacyExtensionFormat(ext);
}

function getLegacyExtensionFormat(ext) {
  if (
    experimentalSpecifierResolution === 'node' &&
    !experimentalSpecifierResolutionWarned
  ) {
    process.emitWarning(
      'The Node.js specifier resolution in ESM is experimental.',
      'ExperimentalWarning');
    experimentalSpecifierResolutionWarned = true;
  }
  return legacyExtensionFormatMap[ext];
}

function getPackageFormat(url, throwIfNotResolved, urlForThrow) {
  let format;

  const ext = extname(url.pathname);
  if (ext === '.js') {
    format = getPackageType(url) === 'module' ? 'module' : 'commonjs';
  } else {
    format = extensionFormatMap[ext] ??
      (throwIfNotResolved ?
        throwIfNotExperimentalSpecifierResolution(ext, urlForThrow) :
        getLegacyExtensionFormat(ext)
      );
  }

  return format;
}

function defaultGetFormat(url, context) {
  const parsed = new URL(url);

  return ObjectPrototypeHasOwnProperty(protocolHandlers, parsed.protocol) ?
    protocolHandlers[parsed.protocol](parsed, url) :
    null;
}

module.exports = {
  defaultGetFormat,
  extensionFormatMap,
  legacyExtensionFormatMap,
  getPackageFormat
};
