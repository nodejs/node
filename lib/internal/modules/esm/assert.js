'use strict';

const {
  ArrayPrototypeFilter,
  ArrayPrototypeIncludes,
  ObjectKeys,
  ObjectValues,
  ObjectPrototypeHasOwnProperty,
} = primordials;
const { validateString } = require('internal/validators');

const {
  ERR_IMPORT_ASSERTION_TYPE_FAILED,
  ERR_IMPORT_ASSERTION_TYPE_MISSING,
  ERR_IMPORT_ASSERTION_TYPE_UNSUPPORTED,
} = require('internal/errors').codes;

// The HTML spec has an implied default type of `'javascript'`.
const kImplicitAssertType = 'javascript';

let alreadyWarned = false;

/**
 * Define a map of module formats to import assertion types (the value of
 * `type` in `assert { type: 'json' }`).
 * @type {Map<string, string>}
 */
const formatTypeMap = {
  '__proto__': null,
  'builtin': kImplicitAssertType,
  'commonjs': kImplicitAssertType,
  'json': 'json',
  'module': kImplicitAssertType,
  'wasm': kImplicitAssertType, // It's unclear whether the HTML spec will require an assertion type or not for Wasm; see https://github.com/WebAssembly/esm-integration/issues/42
};

/**
 * The HTML spec disallows the default type to be explicitly specified
 * (for now); so `import './file.js'` is okay but
 * `import './file.js' assert { type: 'javascript' }` throws.
 * @type {Array<string, string>}
 */
const supportedAssertionTypes = ArrayPrototypeFilter(
  ObjectValues(formatTypeMap),
  (type) => type !== kImplicitAssertType);


/**
 * Test a module's import assertions.
 * @param {string} url The URL of the imported module, for error reporting.
 * @param {string} format One of Node's supported translators
 * @param {Record<string, string>} importAssertions Validations for the
 *                                                  module import.
 * @returns {true}
 * @throws {TypeError} If the format and assertion type are incompatible.
 */
function validateAssertions(url, format,
                            importAssertions = { __proto__: null }) {
  const validType = formatTypeMap[format];

  if (!alreadyWarned && ObjectKeys(importAssertions).length !== 0) {
    alreadyWarned = true;
    process.emitWarning(
      'Import assertions are not a stable feature of the JavaScript language. ' +
      'Avoid relying on their current behavior and syntax as those might change ' +
      'in a future version of Node.js.',
      'ExperimentalWarning',
    );
  }

  switch (validType) {
    case undefined:
      // Ignore assertions for module formats we don't recognize, to allow new
      // formats in the future.
      return true;

    case kImplicitAssertType:
      // This format doesn't allow an import assertion type, so the property
      // must not be set on the import assertions object.
      if (!ObjectPrototypeHasOwnProperty(importAssertions, 'type')) {
        return true;
      }
      return handleInvalidType(url, importAssertions.type);

    case importAssertions.type:
      // The asserted type is the valid type for this format.
      return true;

    default:
      // There is an expected type for this format, but the value of
      // `importAssertions.type` might not have been it.
      if (!ObjectPrototypeHasOwnProperty(importAssertions, 'type')) {
        // `type` wasn't specified at all.
        throw new ERR_IMPORT_ASSERTION_TYPE_MISSING(url, validType);
      }
      return handleInvalidType(url, importAssertions.type);
  }
}

/**
 * Throw the correct error depending on what's wrong with the type assertion.
 * @param {string} url The resolved URL for the module to be imported
 * @param {string} type The value of the import assertion `type` property
 */
function handleInvalidType(url, type) {
  // `type` might have not been a string.
  validateString(type, 'type');

  // `type` might not have been one of the types we understand.
  if (!ArrayPrototypeIncludes(supportedAssertionTypes, type)) {
    throw new ERR_IMPORT_ASSERTION_TYPE_UNSUPPORTED(type);
  }

  // `type` was the wrong value for this format.
  throw new ERR_IMPORT_ASSERTION_TYPE_FAILED(url, type);
}


module.exports = {
  kImplicitAssertType,
  validateAssertions,
};
