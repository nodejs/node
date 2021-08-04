'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('WebCryptoAPI');

// Set Node.js flags required for the tests.
runner.setFlags(['--expose-internals']);

// Set a script that will be executed in the worker before running the tests.
runner.setInitScript(`
  const {
    Crypto,
    SubtleCrypto,
    crypto,
  } = require('internal/crypto/webcrypto');
  const { internalBinding } = require('internal/test/binding');
  const { DOMException } = internalBinding('messaging');
  global.DOMException = DOMException;

  Object.defineProperties(global, {
    Crypto: {
      value: Crypto,
      configurable: true,
      writable: true,
      enumerable: false,
    },
    SubtleCrypto: {
      value: SubtleCrypto,
      configurable: true,
      writable: true,
      enumerable: false,
    },
    CryptoKey: {
      value: crypto.CryptoKey,
      configurable: true,
      writable: true,
      enumerable: false,
    },
    crypto: {
      value: crypto,
      configurable: true,
      writable: true,
      enumerable: false,
    },
  });
`);

runner.runJsTests();
