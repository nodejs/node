'use strict';

const {
  RegExpPrototypeExec,
  decodeURIComponent,
} = primordials;
const { getOptionValue } = require('internal/options');
const { fetch } = require('internal/modules/esm/assets/load');

// Do not eagerly grab .manifest, it may be in TDZ
const policy = getOptionValue('--experimental-policy') ?
  require('internal/process/policy') :
  null;
const experimentalHttpsModules = getOptionValue('--experimental-https-modules');

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
  } else if (experimentalHttpsModules && (
    parsed.protocol === 'https:' ||
    parsed.protocol === 'http:'
  )) {
    const res = await fetch(parsed);
    const source = await res.body;
    return { source };
  } else {
    throw new ERR_UNSUPPORTED_ESM_URL_SCHEME(parsed, [
      'file',
      'data',
      ...(experimentalHttpsModules ? ['https', 'http'] : []),
    ]);
  }
  if (policy?.manifest) {
    policy.manifest.assertIntegrity(parsed, source);
  }
  return { source };
}
exports.defaultGetSource = defaultGetSource;
