'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('WebCryptoAPI');

runner.setInitScript(`
if (Uint8Array.fromHex === undefined) {
  Object.defineProperty(Uint8Array, 'fromHex', {
    __proto__: null,
    configurable: true,
    writable: true,
    value(hex) {
      return new Uint8Array(Buffer.from(hex, 'hex'));
    },
  });
}
`);

runner.pretendGlobalThisAs('Window');

runner.runJsTests();
