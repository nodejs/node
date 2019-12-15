'use strict';

const { Buffer } = require('buffer');

const fs = require('fs');
const { URL } = require('url');
const { promisify } = require('internal/util');
const {
  ERR_INVALID_URL,
  ERR_INVALID_URL_SCHEME,
} = require('internal/errors').codes;
const readFileAsync = promisify(fs.readFile);

const DATA_URL_PATTERN = /^[^/]+\/[^,;]+(?:[^,]*?)(;base64)?,([\s\S]*)$/;

async function defaultGetSource(url, { format } = {}, defaultGetSource) {
  const parsed = new URL(url);
  if (parsed.protocol === 'file:') {
    return {
      source: await readFileAsync(parsed)
    };
  } else if (parsed.protocol === 'data:') {
    const match = DATA_URL_PATTERN.exec(parsed.pathname);
    if (!match) {
      throw new ERR_INVALID_URL(url);
    }
    const [ , base64, body ] = match;
    return {
      source: Buffer.from(body, base64 ? 'base64' : 'utf8')
    };
  } else {
    throw new ERR_INVALID_URL_SCHEME(['file', 'data']);
  }
}
exports.defaultGetSource = defaultGetSource;
