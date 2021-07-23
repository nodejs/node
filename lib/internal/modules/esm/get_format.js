'use strict';
const {
  RegExpPrototypeExec,
  RegExpPrototypeTest,
  StringPrototypeStartsWith,
  PromiseResolve,
  PromisePrototypeThen,
} = primordials;
const { extname } = require('path');
const { getOptionValue } = require('internal/options');
const { fetch } = require('internal/modules/esm/assets/load');

const experimentalJsonModules = getOptionValue('--experimental-json-modules');
const experimentalHttpsModules = getOptionValue('--experimental-https-modules');
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

if (experimentalWasmModules)
  extensionFormatMap['.wasm'] = legacyExtensionFormatMap['.wasm'] = 'wasm';

if (experimentalJsonModules)
  extensionFormatMap['.json'] = legacyExtensionFormatMap['.json'] = 'json';

function mimeToFormat(mime) {
  if (RegExpPrototypeTest(/\s*(text|application)\/javascript\s*(;\s*charset=utf-?8\s*)?/i, mime)) return 'module';
  if (experimentalJsonModules && mime === 'application/json') return 'json';
  if (experimentalWasmModules && mime === 'application/wasm') return 'wasm';
  return null;
}

function defaultGetFormat(url, context, defaultGetFormatUnused) {
  if (StringPrototypeStartsWith(url, 'node:')) {
    return { format: 'builtin' };
  }
  const parsed = new URL(url);
  if (parsed.protocol === 'data:') {
    const { 1: mime } = RegExpPrototypeExec(
      /^([^/]+\/[^;,]+)(?:[^,]*?)(;base64)?,/,
      parsed.pathname,
    ) || [ null, null, null ];
    return { format: mimeToFormat(mime) };
  } else if (experimentalHttpsModules && (
    parsed.protocol === 'https:' ||
    parsed.protocol === 'http:'
  )) {
    return PromisePrototypeThen(
      PromiseResolve(fetch(parsed)),
      (entry) => {
        return {
          format: mimeToFormat(entry.headers['content-type'])
        };
      });
  } else if (parsed.protocol === 'file:') {
    const ext = extname(parsed.pathname);
    let format;
    if (ext === '.js') {
      format = getPackageType(parsed.href) === 'module' ? 'module' : 'commonjs';
    } else {
      format = extensionFormatMap[ext];
    }
    if (!format) {
      if (experimentalSpecifierResolution === 'node') {
        process.emitWarning(
          'The Node.js specifier resolution in ESM is experimental.',
          'ExperimentalWarning');
        format = legacyExtensionFormatMap[ext];
      } else {
        throw new ERR_UNKNOWN_FILE_EXTENSION(ext, fileURLToPath(url));
      }
    }
    return { format: format || null };
  }
  return { format: null };
}

module.exports = {
  defaultGetFormat,
  extensionFormatMap,
  legacyExtensionFormatMap,
};
