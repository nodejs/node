// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const keys = require('internal/crypto/keys');

// This test ensures getKeyObjectHandle throws error for invalid key type
{
  const keyObject = new keys.KeyObject('private', {});
  Object.defineProperty(keyObject, 'type', { get: () => 'test' });

  assert.throws(
    () => {
      keys.preparePublicOrPrivateKey(keyObject);
    },
    {
      name: 'TypeError',
      message: 'Invalid key object type test, expected private or public.',
      code: 'ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE'
    }
  );
}

// This test ensures KeyObject throws error for invalid key type
// during the instantiation
{
  assert.throws(
    () => {
      new keys.KeyObject('someOther', {});
    },
    {
      name: 'TypeError',
      message: "The argument 'type' is invalid. Received 'someOther'",
      code: 'ERR_INVALID_ARG_VALUE'
    }
  );
}

// This test ensures KeyObject throws error for invalid handler variable type
// during the instantiation
{
  assert.throws(
    () => {
      new keys.KeyObject('private', 'notObject');
    },
    {
      name: 'TypeError',
      message:
        'The "handle" argument must be of type string. Received type string',
      code: 'ERR_INVALID_ARG_TYPE'
    }
  );
}
