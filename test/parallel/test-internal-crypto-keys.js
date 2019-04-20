// Flags: --expose-internals
'use strict';
const common = require('../common');
const keys = require('internal/crypto/keys');

// This test ensures getKeyObjectHandle throws error for invalid key type
{
  const keyObject = new keys.KeyObject('private', {});
  Object.defineProperty(keyObject, 'type', { get: () => 'test' });
  common.expectsError(
    () => {
      keys.preparePublicOrPrivateKey(keyObject);
    },
    {
      type: TypeError,
      code: 'ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE',
      message: 'Invalid key object type test, expected private or public.'
    }
  );
}

// This test ensures KeyObject throws error for invalid key type
// during the instantiation
{
  common.expectsError(() => new keys.KeyObject('someOther', {}), {
    type: TypeError,
    code: 'ERR_INVALID_ARG_VALUE',
    message: "The argument 'type' is invalid. Received 'someOther'"
  });
}

// This test ensures KeyObject throws error for invalid handler variable type
// during the instantiation
{
  common.expectsError(() => new keys.KeyObject('private', 'notObject'), {
    type: TypeError,
    code: 'ERR_INVALID_ARG_TYPE',
    message:
      'The "handle" argument must be of type string. Received type string'
  });
}
