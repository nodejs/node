'use strict';

const {
  ArrayPrototypeIncludes,
  ObjectCreate,
  ObjectValues,
  ObjectPrototypeHasOwnProperty,
  Symbol,
} = primordials;
const { validateString } = require('internal/validators');

const {
  ERR_IMPORT_ASSERTION_TYPE_FAILED,
  ERR_IMPORT_ASSERTION_TYPE_MISSING,
  ERR_IMPORT_ASSERTION_TYPE_UNSUPPORTED,
} = require('internal/errors').codes;

const kImplicitAssertType = Symbol('implicit assert type');

/**
 * Define a map of module formats to import assertion types (the value of `type`
 * in `assert { type: 'json' }`).
 * @type {Map<string, string | typeof kImplicitAssertType}
 */
const formatTypeMap = {
  '__proto__': null,
  'builtin': kImplicitAssertType,
  'commonjs': kImplicitAssertType,
  'json': 'json',
  'module': kImplicitAssertType,
  'wasm': kImplicitAssertType, // Should probably be 'webassembly' per https://github.com/tc39/proposal-import-assertions
};

/** @type {Array<string, string | typeof kImplicitAssertType} */
const supportedAssertionTypes = ObjectValues(formatTypeMap);


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
                            importAssertions = ObjectCreate(null)) {
  const validType = formatTypeMap[format];

  switch (validType) {
    case undefined:
      // Ignore assertions for module types we don't recognize, to allow new
      // formats in the future.
      return true;

    case importAssertions.type:
      // The asserted type is the valid type for this format.
      return true;

    case kImplicitAssertType:
      // This format doesn't allow an import assertion type, so the property
      // must not be set on the import assertions object.
      if (!ObjectPrototypeHasOwnProperty(importAssertions, 'type')) {
        return true;
      }
      return handleInvalidType(url, importAssertions.type);

    default:
      // There is an expected type for this format, but the value of
      // `importAssertions.type` was not it.
      if (!ObjectPrototypeHasOwnProperty(importAssertions, 'type')) {
        // `type` wasn't specified at all.
        throw new ERR_IMPORT_ASSERTION_TYPE_MISSING(url, validType);
      }
      handleInvalidType(url, importAssertions.type);
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

  // `type` was not one of the types we understand.
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
