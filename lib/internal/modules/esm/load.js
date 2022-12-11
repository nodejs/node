'use strict';

const {
  RegExpPrototypeExec,
  ArrayPrototypeFilter,
  ArrayPrototypeIncludes,
  ArrayPrototypePush,
  decodeURIComponent,
  ObjectCreate,
  ObjectPrototypeHasOwnProperty,
  ObjectValues,
} = primordials;

const {
  defaultGetFormat,
  formatTypeMap,
  kImplicitAssertType,
} = require('internal/modules/esm/utils');
const { validateString } = require('internal/validators');

const { getOptionValue } = require('internal/options');
const { getLazy } = require('internal/util');
const policy = getLazy(
  () => (getOptionValue('--experimental-policy') ? require('internal/process/policy') : null)
);

const { Buffer: { from: BufferFrom } } = require('buffer');

const { URL } = require('internal/url');
const {
  ERR_INVALID_URL,
  ERR_UNSUPPORTED_ESM_URL_SCHEME,
  ERR_IMPORT_ASSERTION_TYPE_MISSING,
  ERR_IMPORT_ASSERTION_TYPE_FAILED,
  ERR_IMPORT_ASSERTION_TYPE_UNSUPPORTED,
} = require('internal/errors').codes;

const DATA_URL_PATTERN = /^[^/]+\/[^,;]+(?:[^,]*?)(;base64)?,([\s\S]*)$/;

async function getSource(url, context) {
  const parsed = new URL(url);
  let responseURL = url;
  let source;
  const experimentalNetworkImports = getOptionValue('--experimental-network-imports');
  if (parsed.protocol === 'file:') {
    const { readFile: readFileAsync } = require('internal/fs/promises').exports;
    source = await readFileAsync(parsed);
  } else if (parsed.protocol === 'data:') {
    const match = RegExpPrototypeExec(DATA_URL_PATTERN, parsed.pathname);
    if (!match) {
      throw new ERR_INVALID_URL(url);
    }
    const { 1: base64, 2: body } = match;
    source = BufferFrom(decodeURIComponent(body), base64 ? 'base64' : 'utf8');
  } else if (experimentalNetworkImports && (
    parsed.protocol === 'https:' ||
    parsed.protocol === 'http:'
  )) {
    const { fetchModule } = require('internal/modules/esm/fetch_module');
    const res = await fetchModule(parsed, context);
    source = await res.body;
    responseURL = res.resolvedHREF;
  } else {
    const supportedSchemes = ['file', 'data'];
    if (experimentalNetworkImports) {
      ArrayPrototypePush(supportedSchemes, 'http', 'https');
    }
    throw new ERR_UNSUPPORTED_ESM_URL_SCHEME(parsed, supportedSchemes);
  }
  if (policy()?.manifest) {
    policy().manifest.assertIntegrity(parsed, source);
  }
  return { __proto__: null, responseURL, source };
}

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
 * Node.js default load hook.
 * @param {string} url
 * @param {object} context
 * @returns {object}
 */
async function defaultLoad(url, context) {
  let responseURL = url;
  const { importAssertions } = context;
  let {
    format,
    source,
  } = context;

  if (format == null) {
    format = await defaultGetFormat(url, context);
  }

  validateAssertions(url, format, importAssertions);

  if (
    format === 'builtin' ||
    format === 'commonjs'
  ) {
    source = null;
  } else if (source == null) {
    ({ responseURL, source } = await getSource(url, context));
  }

  return {
    __proto__: null,
    format,
    responseURL,
    source,
  };
}

module.exports = {
  defaultLoad,
  validateAssertions,  // for tests.
};
