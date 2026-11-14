// Flags: --js-immutable-arraybuffer
'use strict';

const common = require('../../common');
const assert = require('assert');
const binding = require(`./build/${common.buildType}/test_typedarray`);

for (const bytes of [[1, 2, 3, 4], []]) {
  const buffer = Uint8Array.from(bytes).buffer.transferToImmutable();
  const view = new Uint8Array(buffer);

  assert.throws(() => binding.Detach(view), {
    name: 'Error',
    message: 'A detachable arraybuffer was expected',
  });
  assert.strictEqual(binding.IsDetached(buffer), false);
  assert.strictEqual(buffer.immutable, true);
  assert.strictEqual(view.byteLength, bytes.length);
  assert.deepStrictEqual([...view], bytes);

  const mutable = Uint8Array.from(bytes);
  binding.Detach(mutable);
  assert.strictEqual(binding.IsDetached(mutable.buffer), true);
  assert.strictEqual(mutable.byteLength, 0);
}
