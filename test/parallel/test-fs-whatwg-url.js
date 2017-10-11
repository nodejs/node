'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const os = require('os');
const URL = require('url').URL;

function pathToFileURL(p) {
  if (!path.isAbsolute(p))
    throw new Error('Path must be absolute');
  if (common.isWindows && p.startsWith('\\\\'))
    p = p.slice(2);
  return new URL(`file://${p}`);
}

const p = path.resolve(fixtures.fixturesDir, 'a.js');
const url = pathToFileURL(p);

assert(url instanceof URL);

// Check that we can pass in a URL object successfully
fs.readFile(url, common.mustCall((err, data) => {
  assert.ifError(err);
  assert(Buffer.isBuffer(data));
}));

// Check that using a non file:// URL reports an error
const httpUrl = new URL('http://example.org');
fs.readFile(httpUrl, common.expectsError({
  code: 'ERR_INVALID_URL_SCHEME',
  type: TypeError,
  message: 'The URL must be of scheme file'
}));

// pct-encoded characters in the path will be decoded and checked
fs.readFile(new URL('file:///c:/tmp/%00test'), common.mustCall((err) => {
  common.expectsError(
    () => {
      throw err;
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: Error
    });
}));

if (common.isWindows) {
  // encoded back and forward slashes are not permitted on windows
  ['%2f', '%2F', '%5c', '%5C'].forEach((i) => {
    fs.readFile(new URL(`file:///c:/tmp/${i}`), common.expectsError({
      code: 'ERR_INVALID_FILE_URL_PATH',
      type: TypeError,
      message: 'File URL path must not include encoded \\ or / characters'
    }));
  });
} else {
  // encoded forward slashes are not permitted on other platforms
  ['%2f', '%2F'].forEach((i) => {
    fs.readFile(new URL(`file:///c:/tmp/${i}`), common.expectsError({
      code: 'ERR_INVALID_FILE_URL_PATH',
      type: TypeError,
      message: 'File URL path must not include encoded / characters'
    }));
  });

  fs.readFile(new URL('file://hostname/a/b/c'), common.expectsError({
    code: 'ERR_INVALID_FILE_URL_HOST',
    type: TypeError,
    message: `File URL host must be "localhost" or empty on ${os.platform()}`
  }));
}
