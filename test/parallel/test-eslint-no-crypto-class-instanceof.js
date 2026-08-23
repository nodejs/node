'use strict';

const common = require('../common');
common.skipIfEslintMissing();

const RuleTester = require('../../tools/eslint/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/no-crypto-class-instanceof');

new RuleTester().run('no-crypto-class-instanceof', rule, {
  valid: [
    'value instanceof Buffer;',
    'value instanceof KeyObject;',
    `
    const { isKeyObject } = require('internal/crypto/keys');
    isKeyObject(key);
    `,
    `
    function isCryptoKey(value, CryptoKey) {
      return value instanceof CryptoKey;
    }
    `,
    `
    function isCryptoKey(value, globalThis) {
      return value instanceof globalThis.CryptoKey;
    }
    `,
  ],
  invalid: [
    {
      code: `
      const { KeyObject: KO } = require('internal/crypto/keys');
      key instanceof KO;
      `,
      errors: [{ messageId: 'noKeyObjectInstanceof' }],
    },
    {
      code: `
      const webcrypto = require('internal/crypto/webcrypto');
      key instanceof webcrypto.CryptoKey;
      `,
      errors: [{ messageId: 'noCryptoKeyInstanceof' }],
    },
    {
      code: 'key instanceof globalThis.CryptoKey;',
      errors: [{ messageId: 'noCryptoKeyInstanceof' }],
    },
    {
      code: `
      const { X509Certificate } = require('internal/crypto/x509');
      cert instanceof X509Certificate;
      `,
      errors: [{ messageId: 'noX509CertificateInstanceof' }],
    },
  ],
});
