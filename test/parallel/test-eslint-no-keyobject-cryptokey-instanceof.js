'use strict';

const common = require('../common');
common.skipIfEslintMissing();

const RuleTester = require('../../tools/eslint/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/no-keyobject-cryptokey-instanceof');

new RuleTester().run('no-keyobject-cryptokey-instanceof', rule, {
  valid: [
    'key instanceof Buffer;',
    'key instanceof KeyObject;',
    `
    const { isKeyObject } = require('internal/crypto/keys');
    isKeyObject(key);
    `,
    `
    const { isCryptoKey } = require('internal/crypto/keys');
    isCryptoKey(key);
    `,
  ],
  invalid: [
    {
      code: `
      const { KeyObject } = require('internal/crypto/keys');
      key instanceof KeyObject;
      `,
      errors: [{ messageId: 'noKeyObjectInstanceof' }],
    },
    {
      code: `
      const { KeyObject: KO } = require('internal/crypto/keys');
      key instanceof KO;
      `,
      errors: [{ messageId: 'noKeyObjectInstanceof' }],
    },
    {
      code: `
      const keys = require('internal/crypto/keys');
      key instanceof keys.KeyObject;
      `,
      errors: [{ messageId: 'noKeyObjectInstanceof' }],
    },
    {
      code: `
      key instanceof CryptoKey;
      `,
      errors: [{ messageId: 'noCryptoKeyInstanceof' }],
    },
    {
      code: `
      const { CryptoKey } = require('internal/crypto/keys');
      key instanceof CryptoKey;
      `,
      errors: [{ messageId: 'noCryptoKeyInstanceof' }],
    },
    {
      code: `
      const { CryptoKey: CK } = require('internal/crypto/webcrypto');
      key instanceof CK;
      `,
      errors: [{ messageId: 'noCryptoKeyInstanceof' }],
    },
    {
      code: `
      const webcrypto = require('internal/crypto/webcrypto');
      key instanceof webcrypto.CryptoKey;
      `,
      errors: [{ messageId: 'noCryptoKeyInstanceof' }],
    },
    {
      code: `
      key instanceof globalThis.CryptoKey;
      `,
      errors: [{ messageId: 'noCryptoKeyInstanceof' }],
    },
  ],
});
