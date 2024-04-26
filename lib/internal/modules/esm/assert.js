'use strict';

const {
  ArrayPrototypeFilter,
  ArrayPrototypeFlat,
  ArrayPrototypeIncludes,
  ObjectKeys,
  ObjectPrototypeHasOwnProperty,
  ObjectValues,
} = primordials;
const { validateString } = require('internal/validators');

const {
  ERR_IMPORT_ATTRIBUTE_TYPE_INCOMPATIBLE,
  ERR_IMPORT_ATTRIBUTE_MISSING,
  ERR_IMPORT_ATTRIBUTE_UNSUPPORTED,
} = require('internal/errors').codes;

// The HTML spec has an implied default type of `'javascript'`.
const kImplicitAssertType = 'javascript';

/**
 * Define a map of module formats to import attributes types (the value of
 * `type` in `with { type: 'json' }`).
 * @type {Map<string, string>}
 */
const formatTypeMap = {
  '__proto__': null,
  'builtin': [kImplicitAssertType],
  'commonjs': [kImplicitAssertType],
  'json': ['json', 'jsonc'],
  'module': [kImplicitAssertType],
  'wasm': [kImplicitAssertType], // It's unclear whether the HTML spec will require an attribute type or not for Wasm; see https://github.com/WebAssembly/esm-integration/issues/42
};

/**
 * The HTML spec disallows the default type to be explicitly specified
 * (for now); so `import './file.js'` is okay but
 * `import './file.js' with { type: 'javascript' }` throws.
 * @type {Array<string, string>}
 */
const supportedAssertionTypes = ArrayPrototypeFilter(
  ArrayPrototypeFlat(ObjectValues(formatTypeMap)),
  (type) => type !== kImplicitAssertType);


/**
 * Test a module's import attributes.
 * @param {string} url The URL of the imported module, for error reporting.
 * @param {string} format One of Node's supported translators
 * @param {Record<string, string>} importAttributes Validations for the
 *                                                  module import.
 * @returns {true}
 * @throws {TypeError} If the format and assertion type are incompatible.
 */
function validateAttributes(url, format,
                            importAttributes = { __proto__: null }) {
  const keys = ObjectKeys(importAttributes);
  for (let i = 0; i < keys.length; i++) {
    if (keys[i] !== 'type') {
      throw new ERR_IMPORT_ATTRIBUTE_UNSUPPORTED(keys[i], importAttributes[keys[i]]);
    }
  }
  const validType = formatTypeMap[format];

  if (validType === undefined) {
    return true;
  }

  if (validType[0] === kImplicitAssertType && validType.length === 1) {
    // This format doesn't allow an import assertion type, so the property
    // must not be set on the import attributes object.
    if (!ObjectPrototypeHasOwnProperty(importAttributes, 'type')) {
      return true;
    }
    return handleInvalidType(url, importAttributes.type);
  }

  if (ArrayPrototypeIncludes(validType, importAttributes.type)) {
    // The asserted type is the valid type for this format.
    return true;
  }

  if (!ObjectPrototypeHasOwnProperty(importAttributes, 'type')) {
    // `type` wasn't specified at all.
    throw new ERR_IMPORT_ATTRIBUTE_MISSING(url, 'type', validType);
  }

  return handleInvalidType(url, importAttributes.type);
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
    throw new ERR_IMPORT_ATTRIBUTE_UNSUPPORTED('type', type);
  }

  // `type` was the wrong value for this format.
  throw new ERR_IMPORT_ATTRIBUTE_TYPE_INCOMPATIBLE(url, type);
}


module.exports = {
  kImplicitAssertType,
  validateAttributes,
};
