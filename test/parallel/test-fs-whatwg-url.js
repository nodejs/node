'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');

const url = fixtures.fileURL('a.js');

assert(url instanceof URL);

// Check that we can pass in a URL object successfully
fs.readFile(url, common.mustSucceed((data) => {
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
      });
  });
  assert.throws(
    () => {
      fs.readFile(new URL('file://hostname/a/b/c'), common.mustNotCall());
    },
    {
      code: 'ERR_INVALID_FILE_URL_HOST',
      name: 'TypeError',
    }
  );
  assert.throws(
    () => {
      fs.readFile(new URL('file:///tmp/%00test'), common.mustNotCall());
    },
    {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
    }
  );
}
