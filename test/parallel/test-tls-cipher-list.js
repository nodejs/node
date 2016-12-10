'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const spawn = require('child_process').spawn;
const defaultCoreList = require('crypto').constants.defaultCoreCipherList;

function doCheck(arg, check) {
  var out = '';
  arg = arg.concat([
    '-pe',
    'require("crypto").constants.defaultCipherList'
  ]);
  spawn(process.execPath, arg, {})
    .on('error', common.fail)
    .stdout.on('data', function(chunk) {
      out += chunk;
    }).on('end', function() {
      assert.equal(out.trim(), check);
    }).on('error', common.fail);
}

// test the default unmodified version
doCheck([], defaultCoreList);

// test the command line switch by itself
doCheck(['--tls-cipher-list=ABC'], 'ABC');
