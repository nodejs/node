'use strict';
const common = require('../../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const binding = require(`./build/${common.buildType}/binding`);

// Test 1: Pass a SecureContext to getSSLCtx.
{
  const ctx = tls.createSecureContext();
  assert.strictEqual(binding.getSSLCtx(ctx), true);
}

// Test 2: Passing a non-SecureContext should return nullptr.
{
  assert.strictEqual(binding.getSSLCtxInvalid({}), true);
}

// Test 3: Passing a number should return nullptr.
{
  assert.strictEqual(binding.getSSLCtxInvalid(42), true);
}

// Test 4: Passing undefined should return nullptr.
{
  assert.strictEqual(binding.getSSLCtxInvalid(undefined), true);
}

// Test 5: Passing null should return nullptr.
{
  assert.strictEqual(binding.getSSLCtxInvalid(null), true);
}

// Test 6: An object with a non-SecureContext .context property should return
// nullptr.
{
  assert.strictEqual(binding.getSSLCtxInvalid({ context: 'not a context' }), true);
}

// Test 7: An object with a throwing .context getter should return nullptr
// without propagating the exception.
{
  const obj = {
    get context() { throw new Error('getter threw'); },
  };
  assert.strictEqual(binding.getSSLCtxInvalid(obj), true);
}
