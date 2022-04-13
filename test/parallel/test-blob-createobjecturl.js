// Flags: --no-warnings
'use strict';

const common = require('../common');

// Because registering a Blob URL requires generating a random
// UUID, it can only be done if crypto support is enabled.
if (!common.hasCrypto)
  common.skip('missing crypto');

const {
  URL,
} = require('url');

const {
  Blob,
  resolveObjectURL,
} = require('buffer');

const assert = require('assert');

(async () => {
  const blob = new Blob(['hello']);
  const id = URL.createObjectURL(blob);
  assert.strictEqual(typeof id, 'string');
  const otherBlob = resolveObjectURL(id);
  assert.strictEqual(otherBlob.size, 5);
  assert.strictEqual(
    Buffer.from(await otherBlob.arrayBuffer()).toString(),
    'hello');
  URL.revokeObjectURL(id);

  // should do nothing
  URL.revokeObjectURL(id);

  assert.strictEqual(resolveObjectURL(id), undefined);

  // Leaving a Blob registered should not cause an assert
  // when Node.js exists
  URL.createObjectURL(new Blob());

})().then(common.mustCall());

['not a url', undefined, 1, 'blob:nodedata:1:wrong', {}].forEach((i) => {
  assert.strictEqual(resolveObjectURL(i), undefined);
});

[undefined, 1, '', false, {}].forEach((i) => {
  assert.throws(() => URL.createObjectURL(i), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});
