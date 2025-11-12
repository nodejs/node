'use strict';

const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  assert.fail('When Node.js is compiled without OpenSSL, overriding the global ' +
              'crypto is allowed on string eval');
}

const child = require('child_process');
const nodejs = `"${process.execPath}"`;

// Trying to define a variable named `crypto` using `var` triggers an exception.

child.exec(
  `${nodejs} ` +
      '-p "var crypto = {randomBytes:1};typeof crypto.randomBytes"',
  common.mustSucceed((stdout) => {
    assert.match(stdout, /^number/);
  }));
