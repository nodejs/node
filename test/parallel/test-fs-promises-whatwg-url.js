'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const path = require('path');
const fs = require('fs').promises;
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

(async () => {
  // Check that we can pass in a URL object successfully
  const data = await fs.readFile(url.href);
  assert(Buffer.isBuffer(data));

  try {
    // Check that using a non file:// URL reports an error
    await fs.readFile('http://example.org', common.mustNotCall());
  } catch (err) {
    common.expectsError({
      code: 'ENOENT',
      type: Error,
      message: 'ENOENT: no such file or directory, open \'http://example.org\''
    })(err);
  }

  // pct-encoded characters in the path will be decoded and checked
  if (common.isWindows) {
    // encoded back and forward slashes are not permitted on windows
    await Promise.all(['%2f', '%2F', '%5c', '%5C'].map(async (i) => {
      try {
        await fs.readFile(`file:///c:/tmp/${i}`);
      } catch (err) {
        common.expectsError({
          code: 'ERR_INVALID_FILE_URL_PATH',
          type: TypeError,
          message: 'File URL path must not include encoded \\ or / characters'
        })(err);
      }
    }));

    try {
      await fs.readFile('file:///c:/tmp%00test');
    } catch (err) {
      common.expectsError({
        code: 'ERR_INVALID_ARG_VALUE',
        type: TypeError,
        message: 'The argument \'path\' must be a string or Uint8Array ' +
                 'without null bytes. Received \'c:/tmp/\\u0000test\''
      })(err);
    }
  } else {
    // encoded forward slashes are not permitted on other platforms
    await Promise.all(['%2f', '%2F'].map(async (i) => {
      try {
        await fs.readFile(`file:///c:/tmp/${i}`);
      } catch (err) {
        common.expectsError({
          code: 'ERR_INVALID_FILE_URL_PATH',
          type: TypeError,
          message: 'File URL path must not include encoded / characters'
        })(err);
      }
    }));

    try {
      await fs.readFile('file://hostname/a/b/c');
    } catch (err) {
      common.expectsError({
        code: 'ERR_INVALID_FILE_URL_HOST',
        type: TypeError,
        message: `File URL host must be "localhost" or empty on ${
          os.platform()
        }`
      })(err);
    }

    try {
      fs.readFile('file:///tmp/%00test');
    } catch (err) {
      common.expectsError({
        code: 'ERR_INVALID_ARG_VALUE',
        type: TypeError,
        message: 'The argument \'path\' must be a string or Uint8Array ' +
                 'without null bytes. Received \'/tmp/\\u0000test\''
      })(err);
    }
  }
})().then(common.mustCall());
