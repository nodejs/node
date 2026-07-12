'use strict';

// This tests that url.fileURLToPath() throws ERR_INVALID_FILE_URL_PATH
// for invalid file URL paths along with the input property.

const { isWindows } = require('../common');
const assert = require('assert');
const url = require('url');

const inputs = [];

if (isWindows) {
  inputs.push('file:///C:/a%2F/', 'file:///C:/a%5C/', 'file:///?:/');
} else {
  inputs.push('file:///a%2F/');
}

for (const input of inputs) {
  assert.throws(() => url.fileURLToPath(input), (err) => {
    assert.strictEqual(err.code, 'ERR_INVALID_FILE_URL_PATH');
    assert(err.input instanceof URL);
    assert.strictEqual(err.input.href, input);
    return true;
  });
}
