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

assert.throws(
  () => {
    fs.readFile(httpUrl, common.mustNotCall());
  },
  {
    code: 'ERR_INVALID_URL_SCHEME',
    name: 'TypeError',
    message: 'The URL must be of scheme file'
  });

// pct-encoded characters in the path will be decoded and checked
if (common.isWindows) {
  // Encoded back and forward slashes are not permitted on windows
  ['%2f', '%2F', '%5c', '%5C'].forEach((i) => {
    assert.throws(
      () => {
        fs.readFile(new URL(`file:///c:/tmp/${i}`), common.mustNotCall());
      },
      {
        code: 'ERR_INVALID_FILE_URL_PATH',
        name: 'TypeError',
        message: 'File URL path must not include encoded \\ or / characters'
      }
    );
  });
  assert.throws(
    () => {
      fs.readFile(new URL('file:///c:/tmp/%00test'), common.mustNotCall());
    },
    {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
      message: 'The argument \'path\' must be a string or Uint8Array without ' +
               "null bytes. Received 'c:\\\\tmp\\\\\\x00test'"
    }
  );
} else {
  // Encoded forward slashes are not permitted on other platforms
  ['%2f', '%2F'].forEach((i) => {
    assert.throws(
      () => {
        fs.readFile(new URL(`file:///c:/tmp/${i}`), common.mustNotCall());
      },
      {
        code: 'ERR_INVALID_FILE_URL_PATH',
        name: 'TypeError',
        message: 'File URL path must not include encoded / characters'
      });
  });
  assert.throws(
    () => {
      fs.readFile(new URL('file://hostname/a/b/c'), common.mustNotCall());
    },
    {
      code: 'ERR_INVALID_FILE_URL_HOST',
      name: 'TypeError',
      message: `File URL host must be "localhost" or empty on ${os.platform()}`
    }
  );
  assert.throws(
    () => {
      fs.readFile(new URL('file:///tmp/%00test'), common.mustNotCall());
    },
    {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
      message: "The argument 'path' must be a string or Uint8Array without " +
               "null bytes. Received '/tmp/\\x00test'"
    }
  );
}
