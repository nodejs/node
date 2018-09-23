'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}

const assert = require('assert');
const spawn = require('child_process').spawn;
const defaultCoreList = require('constants').defaultCoreCipherList;

function doCheck(arg, check) {
  var out = '';
  var arg = arg.concat([
    '-pe',
    'require("constants").defaultCipherList'
  ]);
  spawn(process.execPath, arg, {}).
    on('error', common.fail).
    stdout.on('data', function(chunk) {
      out += chunk;
    }).on('end', function() {
      assert.equal(out.trim(), check);
    }).on('error', common.fail);
}

// test the default unmodified version
doCheck([], defaultCoreList);

// test the command line switch by itself
doCheck(['--tls-cipher-list=ABC'], 'ABC');
