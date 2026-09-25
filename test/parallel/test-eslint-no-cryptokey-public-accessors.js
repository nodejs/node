'use strict';

const common = require('../common');
common.skipIfEslintMissing();

const RuleTester = require('../../tools/eslint/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/no-cryptokey-public-accessors');

new RuleTester().run('no-cryptokey-public-accessors', rule, {
  valid: [
    'foo.algorithm;',
    `
    const { isCryptoKey, getCryptoKeyAlgorithm } =
      require('internal/crypto/keys');
    if (isCryptoKey(key)) {
      getCryptoKeyAlgorithm(key);
    }
    `,
    `
    const { CryptoKey } = require('internal/crypto/keys');
    class Key extends CryptoKey {
      get type() { return 'secret'; }
    }
    `,
    `
    key = webidl.converters.KeyFormat(key);
    key.algorithm;
    `,
  ],
  invalid: [
    {
      code: `
      const { isCryptoKey } = require('internal/crypto/keys');
      if (isCryptoKey(key)) {
        key.type;
      }
      `,
      errors: [{ messageId: 'noPublicAccessor' }],
    },
    {
      code: `
      const { isCryptoKey: check } = require('internal/crypto/keys');
      if (check(key) && key.extractable) {}
      `,
      errors: [{ messageId: 'noPublicAccessor' }],
    },
    {
      code: `
      const { isCryptoKey } = require('internal/crypto/keys');
      if (!isCryptoKey(key)) {
        throw new TypeError();
      }
      key.algorithm.name;
      `,
      errors: [{ messageId: 'noPublicAccessor' }],
    },
    {
      code: `
      const keys = require('internal/crypto/keys');
      if (!keys.isCryptoKey(key)) throw new TypeError();
      key['usages'];
      `,
      errors: [{ messageId: 'noPublicAccessor' }],
    },
    {
      code: `
      key = webidl.converters.CryptoKey(key);
      key.algorithm;
      `,
      errors: [{ messageId: 'noPublicAccessor' }],
    },
    {
      code: `
      const key = webidl.converters.CryptoKey(value);
      key.usages;
      `,
      errors: [{ messageId: 'noPublicAccessor' }],
    },
    {
      code: `
      class CryptoKey {
        inspect() { return this.algorithm; }
      }
      `,
      errors: [{ messageId: 'noPublicAccessor' }],
    },
  ],
});
