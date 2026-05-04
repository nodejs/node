'use strict';

const common = require('../common');
common.skipIfEslintMissing();

const RuleTester = require('../../tools/eslint/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/no-keyobject-public-accessors');

new RuleTester().run('no-keyobject-public-accessors', rule, {
  valid: [
    'foo.type;',
    `
    const { isKeyObject, getKeyObjectType } =
      require('internal/crypto/keys');
    if (isKeyObject(key)) {
      getKeyObjectType(key);
    }
    `,
    `
    const { isKeyObject } = require('internal/crypto/keys');
    if (format === 'raw-public') {
      key.asymmetricKeyType;
    }
    `,
    `
    const { KeyObject } = require('internal/crypto/keys');
    class Key extends KeyObject {
      get type() { return 'secret'; }
    }
    `,
  ],
  invalid: [
    {
      code: `
      const { isKeyObject } = require('internal/crypto/keys');
      if (isKeyObject(key)) {
        key.type;
      }
      `,
      errors: [{ messageId: 'noPublicAccessor' }],
    },
    {
      code: `
      const { isKeyObject: check } = require('internal/crypto/keys');
      if (check(key) && key.symmetricKeySize === 32) {}
      `,
      errors: [{ messageId: 'noPublicAccessor' }],
    },
    {
      code: `
      const { isKeyObject } = require('internal/crypto/keys');
      if (!isKeyObject(otherKeyObject)) {
        throw new TypeError();
      }
      otherKeyObject.asymmetricKeyType;
      `,
      errors: [{ messageId: 'noPublicAccessor' }],
    },
    {
      code: `
      const keys = require('internal/crypto/keys');
      if (!keys.isKeyObject(otherKeyObject)) throw new TypeError();
      otherKeyObject.asymmetricKeyDetails;
      `,
      errors: [{ messageId: 'noPublicAccessor' }],
    },
    {
      code: `
      class SecretKeyObject extends KeyObject {
        export() { return this.symmetricKeySize; }
      }
      `,
      errors: [{ messageId: 'noPublicAccessor' }],
    },
    {
      code: `
      const { isKeyObject } = require('internal/crypto/keys');
      if (isKeyObject(key)) {
        key.equals(otherKey);
      }
      `,
      errors: [{ messageId: 'noPublicAccessor' }],
    },
  ],
});
