// Flags: --js-immutable-arraybuffer
'use strict';
const common = require('../../common');
const assert = require('node:assert/strict');

if (typeof ArrayBuffer.prototype.transferToImmutable !== 'function')
  common.skip('ArrayBuffer.prototype.transferToImmutable is not available');

const { Detach, IsDetached } = require(`./build/${common.buildType}/test_typedarray`);

const bytes = [1, 2, 3];
const buffer = Uint8Array.from(bytes).buffer.transferToImmutable();
const view = new Uint8Array(buffer);

assert.throws(() => Detach(view), {
  message: 'A detachable arraybuffer was expected',
});
assert.equal(IsDetached(buffer), false);
assert.deepEqual([...view], bytes);
