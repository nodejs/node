'use strict';

const {
  ArrayPrototypePush,
  RegExpPrototypeExec,
  decodeURIComponent,
} = primordials;
const { kEmptyObject } = require('internal/util');

const { defaultGetFormat } = require('internal/modules/esm/get_format');
const { validateAssertions } = require('internal/modules/esm/assert');
const { getOptionValue } = require('internal/options');

// Do not eagerly grab .manifest, it may be in TDZ
const policy = getOptionValue('--experimental-policy') ?
  require('internal/process/policy') :
  null;
const experimentalNetworkImports =
  getOptionValue('--experimental-network-imports');

const { Buffer: { from: BufferFrom } } = require('buffer');

const { URL } = require('internal/url');
const {
  ERR_INVALID_URL,
  ERR_UNKNOWN_MODULE_FORMAT,
  ERR_UNSUPPORTED_ESM_URL_SCHEME,
} = require('internal/errors').codes;

const DATA_URL_PATTERN = /^[^/]+\/[^,;]+(?:[^,]*?)(;base64)?,([\s\S]*)$/;

async function getSource(url, context) {
  const parsed = new URL(url);
  let responseURL = url;
  let source;
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
  if (policy?.manifest) {
    policy.manifest.assertIntegrity(parsed, source);
  }
  return { __proto__: null, responseURL, source };
}


/**
 * Node.js default load hook.
 * @param {string} url
 * @param {object} context
 * @returns {object}
 */
async function defaultLoad(url, context = kEmptyObject) {
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


/**
 * For a falsy `format` returned from `load`, throw an error.
 * This could happen from either a custom user loader _or_ from the default loader, because the default loader tries to
 * determine formats for data URLs.
 * @param {string} url The resolved URL of the module
 * @param {null | undefined | false | 0 | -0 | 0n | ''} format Falsy format returned from `load`
 */
function throwUnknownModuleFormat(url, format) {
  const dataUrl = RegExpPrototypeExec(
    /^data:([^/]+\/[^;,]+)(?:[^,]*?)(;base64)?,/,
    url,
  );

  throw new ERR_UNKNOWN_MODULE_FORMAT(
    dataUrl ? dataUrl[1] : format,
    url);
}


module.exports = {
  defaultLoad,
  throwUnknownModuleFormat,
};
