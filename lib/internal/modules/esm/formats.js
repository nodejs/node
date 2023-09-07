'use strict';

const {
  RegExpPrototypeExec,
  Uint8Array,
} = primordials;
const { getOptionValue } = require('internal/options');

const { closeSync, openSync, readSync } = require('fs');

const experimentalWasmModules = getOptionValue('--experimental-wasm-modules');

const extensionFormatMap = {
  '__proto__': null,
  '.cjs': 'commonjs',
  '.js': 'module',
  '.json': 'json',
  '.mjs': 'module',
};

if (experimentalWasmModules) {
  extensionFormatMap['.wasm'] = 'wasm';
}

/**
 * @param {string} mime
 * @returns {string | null}
 */
function mimeToFormat(mime) {
  if (
    RegExpPrototypeExec(
      /^\s*(text|application)\/javascript\s*(;\s*charset=utf-?8\s*)?$/i,
      mime,
    ) !== null
  ) return 'module';
  if (mime === 'application/json') return 'json';
  if (experimentalWasmModules && mime === 'application/wasm') return 'wasm';
  return null;
}

function guessExtensionlessModule(url) {
  if (!experimentalWasmModules)
    return 'module';

  const magic = new Uint8Array(4);
  let fd;
  try {
    fd = openSync(url);
    readSync(fd, magic);
    if (magic[0] === 0x00 && magic[1] === 0x61 && magic[2] === 0x73 && magic[3] === 0x6d) {
      return 'wasm';
    }
  } finally {
    if (fd) closeSync(fd);
  }

  return 'module';
}

module.exports = {
  extensionFormatMap,
  guessExtensionlessModule,
  mimeToFormat,
};
