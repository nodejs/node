'use strict';

const {
  ArrayPrototypeConcat,
  RegExpPrototypeExec,
  decodeURIComponent,
} = primordials;
const { getOptionValue } = require('internal/options');
const { fetchModule } = require('internal/modules/esm/fetch_module');

// Do not eagerly grab .manifest, it may be in TDZ
const policy = getOptionValue('--experimental-policy') ?
  require('internal/process/policy') :
  null;
const experimentalNetworkImports =
  getOptionValue('--experimental-network-imports');

const { Buffer: { from: BufferFrom } } = require('buffer');

const fs = require('internal/fs/promises').exports;
const { URL } = require('internal/url');
const {
  ERR_INVALID_URL,
  ERR_UNSUPPORTED_ESM_URL_SCHEME,
} = require('internal/errors').codes;
const readFileAsync = fs.readFile;

const DATA_URL_PATTERN = /^[^/]+\/[^,;]+(?:[^,]*?)(;base64)?,([\s\S]*)$/;

async function defaultGetSource(url, context, defaultGetSource) {
  const parsed = new URL(url);
  let source;
  if (parsed.protocol === 'file:') {
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
    const res = await fetchModule(parsed, context);
    source = await res.body;
  } else {
    throw new ERR_UNSUPPORTED_ESM_URL_SCHEME(parsed, ArrayPrototypeConcat([
      'file',
      'data',
      experimentalNetworkImports ? ['https', 'http'] : [],
    ]));
  }
  if (policy?.manifest) {
    policy.manifest.assertIntegrity(parsed, source);
  }
  return source;
}
exports.defaultGetSource = defaultGetSource;
