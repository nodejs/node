'use strict';
const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

// This tests crypto.setEngine().

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

assert.throws(() => crypto.setEngine(true), /ERR_INVALID_ARG_TYPE/);
assert.throws(() => crypto.setEngine('/path/to/engine', 'notANumber'),
              /ERR_INVALID_ARG_TYPE/);

{
  const invalidEngineName = 'xxx';
  assert.throws(() => crypto.setEngine(invalidEngineName),
                /ERR_CRYPTO_ENGINE_UNKNOWN/);
  assert.throws(() => crypto.setEngine(invalidEngineName,
                                       crypto.constants.ENGINE_METHOD_RSA),
                /ERR_CRYPTO_ENGINE_UNKNOWN/);
}

crypto.setEngine('dynamic');
crypto.setEngine('dynamic');

crypto.setEngine('dynamic', crypto.constants.ENGINE_METHOD_RSA);
crypto.setEngine('dynamic', crypto.constants.ENGINE_METHOD_RSA);

{
  const engineName = 'test_crypto_engine';
  let engineLib;
  if (common.isOSX)
    engineLib = `lib${engineName}.dylib`;
  else if (common.isLinux && process.arch === 'x64')
    engineLib = `lib${engineName}.so`;

  if (engineLib !== undefined) {
    const execDir = path.dirname(process.execPath);
    const enginePath = path.join(execDir, engineLib);
    const engineId = path.parse(engineLib).name;

    fs.accessSync(enginePath);

    crypto.setEngine(enginePath);
    // OpenSSL 3.0.1 and 1.1.1m now throw errors if an engine is loaded again
    // with a duplicate absolute path.
    // TODO(richardlau): figure out why this fails on macOS but not Linux.
    // crypto.setEngine(enginePath);

    // crypto.setEngine(enginePath, crypto.constants.ENGINE_METHOD_RSA);
    // crypto.setEngine(enginePath, crypto.constants.ENGINE_METHOD_RSA);

    process.env.OPENSSL_ENGINES = execDir;

    crypto.setEngine(engineId);
    crypto.setEngine(engineId);

    crypto.setEngine(engineId, crypto.constants.ENGINE_METHOD_RSA);
    crypto.setEngine(engineId, crypto.constants.ENGINE_METHOD_RSA);
  }
}
