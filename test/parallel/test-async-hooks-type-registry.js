'use strict';
const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');

const types1 = async_hooks.getTypes();
types1.forEach((v, k) => assert.strictEqual(
  typeof v,
  'string',
  `${v} from types[${k}] should be a string`)
);

const sillyName = 'gaga';
const newType = async_hooks.registerTypeName(sillyName);
assert.strictEqual(
  typeof newType,
  'string',
  `${newType} should be a string`
);
assert.strictEqual(newType, sillyName);
assert.throws(
  () => async_hooks.registerTypeName(sillyName),
  common.expectsError(
    {
      code: 'ERR_ASYNC_PROVIDER_NAME',
      type: TypeError,
      message: `"${sillyName}" type name already registered`
    }
  )
);

const types2 = async_hooks.getTypes();
assert.strictEqual(types2.length, types1 + 1);
assert.strictEqual(types2.includes(newType), true);
