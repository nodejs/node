'use strict';
const common = require('../common');

// This test ensures that a dynamic engine can be registered with
// crypto.setEngine() and crypto.getEngines() obtains the list for
// both builtin and dynamic engines.

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

let found = 0;

// check builtin engine exists.
// A dynamic engine is loaded by ENGINE_load_builtin_engines()
// in InitCryptoOnce().
crypto.getEngines().forEach((e) => {
  if (e.id === 'dynamic')
    found++;
});

assert.strictEqual(found, 1);

// check set and get node test engine of
// test/fixtures/openssl_test_engine/node_test_engine.c

if (process.config.variables.node_shared_openssl) {
  common.skip('node test engine cannot be built in shared openssl');
  return;
}

if (process.config.variables.openssl_fips) {
  common.skip('node test engine cannot be built in FIPS mode.');
  return;
}

let engine_lib;

if (common.isWindows) {
  engine_lib = 'node_test_engine.dll';
} else if (common.isAix) {
  engine_lib = 'libnode_test_engine.a';
} else if (process.platform === 'darwin') {
  engine_lib = 'node_test_engine.so';
} else {
  engine_lib = 'libnode_test_engine.so';
}

const test_engine_id = 'node_test_engine';
const test_engine_name = 'Node Test Engine for OpenSSL';
const test_engine = path.join(path.dirname(process.execPath), engine_lib);

assert.doesNotThrow(function() {
  fs.accessSync(test_engine);
}, 'node test engine ' + test_engine + ' is not found.');

crypto.setEngine(test_engine);

crypto.getEngines().forEach((e) => {
  if (e.id === test_engine_id && e.name === test_engine_name &&
      e.flags === crypto.constants.ENGINE_METHOD_ALL)
    found++;
});

assert.strictEqual(found, 2);

// Error Tests for setEngine
assert.throws(function() {
  crypto.setEngine(true);
}, /^TypeError: "id" argument should be a string$/);

assert.throws(function() {
  crypto.setEngine('/path/to/engine', 'notANumber');
}, /^TypeError: "flags" argument should be a number, if present$/);
