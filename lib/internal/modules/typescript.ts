'use strict';

type Options = import('../../../deps/amaro/lib/wasm').Options;
type TransformOutput = import('../../../deps/amaro/lib/wasm').TransformOutput;
type Mode = 'strip-only' | 'transform';

const {
  ObjectPrototypeHasOwnProperty,
} = primordials;
const {
  validateBoolean,
  validateOneOf,
  validateObject,
  validateString,
} = require('internal/validators');
const {
  assertTypeScript,
  emitExperimentalWarning,
  getLazy,
  isUnderNodeModules,
  kEmptyObject
} = require('internal/util');
const {
  ERR_INVALID_TYPESCRIPT_SYNTAX,
  ERR_UNSUPPORTED_NODE_MODULES_TYPE_STRIPPING,
  ERR_UNSUPPORTED_TYPESCRIPT_SYNTAX,
} = require('internal/errors').codes;
const { getOptionValue } = require('internal/options');
const assert = require('internal/assert');
const { Buffer } = require('buffer');
const {
  getCompileCacheEntry,
  saveCompileCacheEntry,
  cachedCodeTypes: { kStrippedTypeScript, kTransformedTypeScript, kTransformedTypeScriptWithSourceMaps },
} = internalBinding('modules');

/**
 * The TypeScript parsing mode, either 'strip-only' or 'transform'.
 */
const getTypeScriptParsingMode: () => Mode = getLazy(() =>
(getOptionValue('--experimental-transform-types') ?
  (emitExperimentalWarning('Transform Types'), 'transform') : 'strip-only'),
);

/**
 * Load the TypeScript parser.
 * and returns an object with a `code` property.
 */
const loadTypeScriptParser: () => (source: string, options: Options) => TransformOutput = getLazy(() => {
  assertTypeScript();
  const amaro: { transformSync: (source: string, options: Options) => TransformOutput } = require('internal/deps/amaro/dist/index');
  return amaro.transformSync;
});

/**
 * Parse TypeScript source code.
 */
function parseTypeScript(source: string, options: Options): TransformOutput {
  const parse = loadTypeScriptParser();
  try {
    return parse(source, options);
  } catch (error) {
    /**
     * Amaro v0.3.0 (from SWC v1.10.7) throws an object with `message` and `code` properties.
     * It allows us to distinguish between invalid syntax and unsupported syntax.
     */
    switch (error?.code) {
      case 'UnsupportedSyntax': {
        const unsupportedSyntaxError = new ERR_UNSUPPORTED_TYPESCRIPT_SYNTAX(error.message);
        throw decorateErrorWithSnippet(unsupportedSyntaxError, error); /* node-do-not-add-exception-line */
      }
      case 'InvalidSyntax': {
        const invalidSyntaxError = new ERR_INVALID_TYPESCRIPT_SYNTAX(error.message);
        throw decorateErrorWithSnippet(invalidSyntaxError, error); /* node-do-not-add-exception-line */
      }
      default:
        // SWC may throw strings when something goes wrong.
        if (typeof error === 'string') { assert.fail(error); }
        assert(error != null && ObjectPrototypeHasOwnProperty(error, 'message'));
        assert.fail(error.message);
    }
  }
}

/**
 * Decorate an error with a code snippet from the Amaro error.
 */
function decorateErrorWithSnippet(error: Error, amaroError: any): Error {
  const errorHints = `${amaroError.filename}:${amaroError.startLine}\n${amaroError.snippet}`;
  error.stack = `${errorHints}\n${error.stack}`;
  return error;
}

/**
 * Performs type-stripping to TypeScript source code.
 */
function stripTypeScriptTypes(code: string, options = kEmptyObject): string {
  emitExperimentalWarning('stripTypeScriptTypes');
  validateString(code, 'code');
  validateObject(options, 'options');

  const {
    sourceMap = false,
    sourceUrl = '',
  } = options;
  let { mode = 'strip' } = options;
  validateOneOf(mode, 'options.mode', ['strip', 'transform']);
  validateBoolean(sourceMap, 'options.sourceMap');
  validateString(sourceUrl, 'options.sourceUrl');
  if (mode === 'strip') {
    validateOneOf(sourceMap, 'options.sourceMap', [false, undefined]);
    // Rename mode from 'strip' to 'strip-only'.
    // The reason is to match `process.features.typescript` which returns `strip`,
    // but the parser expects `strip-only`.
    mode = 'strip-only';
  }

  return processTypeScriptCode(code, {
    mode,
    sourceMap,
    filename: sourceUrl,
  });
}

/**
 * Processes TypeScript code by stripping types or transforming.
 * Handles source maps if needed.
 */
function processTypeScriptCode(code: string, options: Options): string {
  const { code: transformedCode, map } = parseTypeScript(code, options);

  if (map) {
    return addSourceMap(transformedCode, map);
  }

  if (options.filename) {
    return `${transformedCode}\n\n//# sourceURL=${options.filename}`;
  }

  return transformedCode;
}

/**
 * Get the type enum used for compile cache.
 */
function getCachedCodeType(mode: Mode, sourceMap: boolean): number {
  if (mode === 'transform') {
    if (sourceMap) { return kTransformedTypeScriptWithSourceMaps; }
    return kTransformedTypeScript;
  }
  return kStrippedTypeScript;
}

/**
 * Performs type-stripping to TypeScript source code internally.
 * It is used by internal loaders.
 */
function stripTypeScriptModuleTypes(source: string, filename: string): string {
  assert(typeof source === 'string');
  if (isUnderNodeModules(filename)) {
    throw new ERR_UNSUPPORTED_NODE_MODULES_TYPE_STRIPPING(filename);
  }
  const sourceMap = getOptionValue('--enable-source-maps');

  const mode = getTypeScriptParsingMode();

  // Instead of caching the compile cache status, just go into C++ to fetch it,
  // as checking process.env equally involves calling into C++ anyway, and
  // the compile cache can be enabled dynamically.
  const type = getCachedCodeType(mode, sourceMap);
  // Get a compile cache entry into the native compile cache store,
  // keyed by the filename. If the cache can already be loaded on disk,
  // cached.transpiled contains the cached string. Otherwise we should do
  // the transpilation and save it in the native store later using
  // saveCompileCacheEntry().
  const cached = (filename ? getCompileCacheEntry(source, filename, type) : undefined);
  if (cached?.transpiled) {  // TODO(joyeecheung): return Buffer here.
    return cached.transpiled;
  }

  const options: Options = {
    mode,
    sourceMap,
    filename,
  };

  const transpiled = processTypeScriptCode(source, options);
  if (cached) {
    // cached.external contains a pointer to the native cache entry.
    // The cached object would be unreachable once it's out of scope,
    // but the pointer inside cached.external would stay around for reuse until
    // environment shutdown or when the cache is manually flushed
    // to disk. Unwrap it in JS before passing into C++ since it's faster.
    saveCompileCacheEntry(cached.external, transpiled);
  }
  return transpiled;
}

/**
 * Add a source map to compiled code.
 */
function addSourceMap(code: string, sourceMap: string): string {
  // The base64 encoding should be https://datatracker.ietf.org/doc/html/rfc4648#section-4,
  // not base64url https://datatracker.ietf.org/doc/html/rfc4648#section-5. See data url
  // spec https://tools.ietf.org/html/rfc2397#section-2.
  const base64SourceMap = Buffer.from(sourceMap).toString('base64');
  return `${code}\n\n//# sourceMappingURL=data:application/json;base64,${base64SourceMap}`;
}

module.exports = {
  stripTypeScriptModuleTypes,
  stripTypeScriptTypes,
};
