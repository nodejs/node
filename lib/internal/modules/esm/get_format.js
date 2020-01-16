'use strict';

const { NativeModule } = require('internal/bootstrap/loaders');
const { extname } = require('path');
const { getOptionValue } = require('internal/options');

const experimentalJsonModules = getOptionValue('--experimental-json-modules');
const experimentalSpeciferResolution =
  getOptionValue('--experimental-specifier-resolution');
const experimentalWasmModules = getOptionValue('--experimental-wasm-modules');
const { getPackageType } = internalBinding('module_wrap');
const { URL, fileURLToPath } = require('internal/url');
const { ERR_UNKNOWN_FILE_EXTENSION } = require('internal/errors').codes;

// const TYPE_NONE = 0;
// const TYPE_COMMONJS = 1;
// const TYPE_MODULE = 2;
// const TYPE_WASM = 3;

const wasmFormatMap = {
  '__proto__': null,
  '.cjs': 'commonjs',
  '.js': 'module',
  '.mjs': 'module',
};

const moduleFormatMap = {
  '__proto__': null,
  '': 'module',
  '.cjs': 'commonjs',
  '.js': 'module',
  '.mjs': 'module'
};

const legacyExtensionFormatMap = {
  '__proto__': null,
  '': 'commonjs',
  '.cjs': 'commonjs',
  '.js': 'commonjs',
  '.json': 'commonjs',
  '.mjs': 'module',
  '.node': 'commonjs'
};

const formatMaps = [
  legacyExtensionFormatMap,
  legacyExtensionFormatMap,
  moduleFormatMap,
  experimentalWasmModules ? wasmFormatMap : legacyExtensionFormatMap
];

if (experimentalWasmModules) {
  wasmFormatMap[''] =
  wasmFormatMap['.wasm'] =
  moduleFormatMap['.wasm'] =
  legacyExtensionFormatMap['.wasm'] = 'wasm';
}

if (experimentalJsonModules) {
  wasmFormatMap['.json'] =
  moduleFormatMap['.json'] =
  legacyExtensionFormatMap['.json'] = 'json';
}

function defaultGetFormat(url, context, defaultGetFormat) {
  if (NativeModule.canBeRequiredByUsers(url)) {
    return { format: 'builtin' };
  }
  const parsed = new URL(url);
  if (parsed.protocol === 'data:') {
    const [ , mime ] = /^([^/]+\/[^;,]+)(?:[^,]*?)(;base64)?,/.exec(parsed.pathname) || [ null, null, null ];
    const format = ({
      '__proto__': null,
      'text/javascript': 'module',
      'application/json': experimentalJsonModules ? 'json' : null,
      'application/wasm': experimentalWasmModules ? 'wasm' : null
    })[mime] || null;
    return { format };
  } else if (parsed.protocol === 'file:') {
    const ext = extname(parsed.pathname);
    let format;
    if (ext === '.js' || ext === '') {
      const type = getPackageType(parsed.href);
      format = formatMaps[type][ext];
    } else {
      format = moduleFormatMap[ext];
    }
    if (!format) {
      if (experimentalSpeciferResolution === 'node') {
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
}
exports.defaultGetFormat = defaultGetFormat;
