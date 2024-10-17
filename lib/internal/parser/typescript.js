'use strict';

const {
  validateBoolean,
  validateOneOf,
  validateObject,
  validateString,
} = require('internal/validators');
const { assertTypeScript, emitExperimentalWarning, getLazy, kEmptyObject } = require('internal/util');
const {
  ERR_INVALID_TYPESCRIPT_SYNTAX,
} = require('internal/errors').codes;
const { getOptionValue } = require('internal/options');
const { Buffer } = require('buffer');
const assert = require('internal/assert');

/**
 * The TypeScript parsing mode, either 'strip-only' or 'transform'.
 * @type {string}
 */
const getTypeScriptParsingMode = getLazy(() =>
  (getOptionValue('--experimental-transform-types') ? 'transform' : 'strip-only'),
);

/**
 * Load the TypeScript parser.
 * and returns an object with a `code` property.
 * @returns {Function} The TypeScript parser function.
 */
const loadTypeScriptParser = getLazy(() => {
  assertTypeScript();
  const amaro = require('internal/deps/amaro/dist/index');
  return amaro.transformSync;
});

/**
 *
 * @param {string} source the source code
 * @param {object} options the options to pass to the parser
 * @returns {TransformOutput} an object with a `code` property.
 */
function parseTypeScript(source, options) {
  const parse = loadTypeScriptParser();
  try {
    return parse(source, options);
  } catch (error) {
    throw new ERR_INVALID_TYPESCRIPT_SYNTAX(error);
  }
}

/**
 * @typedef {object} TransformOptions
 * @property {string} mode The TypeScript parsing mode, either 'strip-only' or 'transform'.
 * @property {boolean} sourceMap Whether to generate source-maps.
 * @property {string} filename The filename of the source code.
 *
 * Performs type-stripping to TypeScript source code.
 * @param {string} code TypeScript code to parse.
 * @param {TransformOptions} options The configuration for type stripping.
 * @returns {string} The stripped TypeScript code.
 */
function stripTypeScriptTypes(code, options = kEmptyObject) {
  emitExperimentalWarning('stripTypeScriptTypes');
  validateObject(options, 'options');
  const { mode = 'strip-only', sourceMap = false, sourceUrl = '' } = options;
  validateOneOf(mode, 'options.mode', ['strip-only', 'transform']);
  if (mode === 'strip-only') {
    validateOneOf(sourceMap, 'options.sourceMap', [false, undefined]);
  }
  validateBoolean(sourceMap, 'options.sourceMap');
  validateString(sourceUrl, 'options.sourceUrl');

  const transformOptions = {
    __proto__: null,
    mode,
    sourceMap,
    filename: sourceUrl,
  };

  const { code: transformed, map } = parseTypeScript(code, transformOptions);
  if (map) {
    return addSourceMap(transformed, map);
  }

  if (sourceUrl) {
    return `${transformed}\n\n//# sourceURL=${sourceUrl}`;
  }

  return transformed;
}

/**
 *
 * @param {string} code The compiled code.
 * @param {string} sourceMap The source map.
 * @returns {string} The code with the source map attached.
 */
function addSourceMap(code, sourceMap) {
  // TODO(@marco-ippolito) When Buffer.transcode supports utf8 to
  // base64 transformation, we should change this line.
  const base64SourceMap = Buffer.from(sourceMap).toString('base64');
  return `${code}\n\n//# sourceMappingURL=data:application/json;base64,${base64SourceMap}`;
}

/**
 * @typedef {object} TransformOutput
 * @property {string} code The compiled code.
 * @property {string} [map] The source maps (optional).
 *
 * Performs type-stripping to TypeScript source code.
 * @param {string} source TypeScript code to parse.
 * @param {string} filename The filename of the source code.
 * @returns {TransformOutput} The stripped TypeScript code.
 */
function stripTypeScriptModuleTypes(source, filename) {
  assert(typeof source === 'string');
  const options = {
    __proto__: null,
    mode: getTypeScriptParsingMode(),
    sourceMap: getOptionValue('--enable-source-maps'),
    filename,
  };
  const { code, map } = parseTypeScript(source, options);
  if (map) {
    return addSourceMap(code, map);
  }
  // Source map is not necessary in strip-only mode. However, to map the source
  // file in debuggers to the original TypeScript source, add a sourceURL magic
  // comment to hint that it is a generated source.
  return `${code}\n\n//# sourceURL=${filename}`;
}

module.exports = {
  stripTypeScriptModuleTypes,
  stripTypeScriptTypes,
};
