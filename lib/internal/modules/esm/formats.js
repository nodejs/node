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

const legacyExtensionFormatMap = {
  '__proto__': null,
  '.cjs': 'commonjs',
  '.js': 'commonjs',
  '.json': 'commonjs',
  '.mjs': 'module',
  '.node': 'commonjs',
};

if (experimentalWasmModules) {
  extensionFormatMap['.wasm'] = legacyExtensionFormatMap['.wasm'] = 'wasm';
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
  ) { return 'module'; }
  if (mime === 'application/json') { return 'json'; }
  if (experimentalWasmModules && mime === 'application/wasm') { return 'wasm'; }
  return null;
}

function getLegacyExtensionFormat(ext) {
  return legacyExtensionFormatMap[ext];
}

/**
 * For extensionless files in a `module` package scope, or a default `module` scope enabled by the
 * `--experimental-default-type` flag, we check the file contents to disambiguate between ES module JavaScript and Wasm.
 * We do this by taking advantage of the fact that all Wasm files start with the header `0x00 0x61 0x73 0x6d` (`_asm`).
 * @param {URL} url
 */
function getFormatOfExtensionlessFile(url) {
  if (!experimentalWasmModules) { return 'module'; }

  const magic = new Uint8Array(4);
  let fd;
  try {
    // TODO(@anonrig): Optimize the following by having a single C++ call
    fd = openSync(url);
    readSync(fd, magic, 0, 4); // Only read the first four bytes
    if (magic[0] === 0x00 && magic[1] === 0x61 && magic[2] === 0x73 && magic[3] === 0x6d) {
      return 'wasm';
    }
  } finally {
    if (fd !== undefined) { closeSync(fd); }
  }

  return 'module';
}

module.exports = {
  extensionFormatMap,
  getFormatOfExtensionlessFile,
  getLegacyExtensionFormat,
  legacyExtensionFormatMap,
  mimeToFormat,
};
