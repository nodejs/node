'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { join } = require('node:path');
const { hasFIPS } = require('../common/crypto');
const { WPTRunner } = require('../common/wpt');

// Runs each spec in its own process; this suite has crashed the runner in CI.
const runner = new WPTRunner('WebCryptoAPI', { backend: 'process' });

runner.pretendGlobalThisAs('Window');

if (hasFIPS(3, 5)) {
  const supportsFile = join(
    'WebCryptoAPI',
    'supports.tentative.https.any.js');
  const eagerX25519Key = `    deriveBitsParams: {
      name: 'X25519',
      public: crypto.subtle.generateKey('X25519', false, ['deriveBits']),
    },`;
  const unavailableX25519Key = `    deriveBitsParams: {
      name: 'X25519',
      public: undefined,
    },`;
  runner.setScriptModifier((script) => {
    if (!script.filename.endsWith(supportsFile))
      return;

    const fragments = script.code.split(eagerX25519Key);
    if (fragments.length !== 2) {
      throw new Error(
        `Expected exactly one eager X25519 key in ${script.filename}; ` +
        `found ${fragments.length - 1}`);
    }
    script.code = fragments.join(unavailableX25519Key);
  });
}

runner.runJsTests();
