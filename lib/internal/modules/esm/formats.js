'use strict';

const {
  RegExpPrototypeTest,
} = primordials;

const extensionFormatMap = {
  '__proto__': null,
  '.cjs': 'commonjs',
  '.js': 'module',
  '.json': 'json',
  '.mjs': 'module',
  '.wasm': 'wasm',
};

const legacyExtensionFormatMap = {
  '__proto__': null,
  '.cjs': 'commonjs',
  '.js': 'commonjs',
  '.json': 'commonjs',
  '.mjs': 'module',
  '.node': 'commonjs',
  '.wasm': 'wasm',
};

function mimeToFormat(mime) {
  if (
    RegExpPrototypeTest(
      /\s*(text|application)\/javascript\s*(;\s*charset=utf-?8\s*)?/i,
      mime
    )
  ) return 'module';
  if (mime === 'application/json') return 'json';
  if (mime === 'application/wasm') return 'wasm';
  return null;
}

function getLegacyExtensionFormat(ext) {
  return legacyExtensionFormatMap[ext];
}

module.exports = {
  extensionFormatMap,
  getLegacyExtensionFormat,
  legacyExtensionFormatMap,
  mimeToFormat,
};
