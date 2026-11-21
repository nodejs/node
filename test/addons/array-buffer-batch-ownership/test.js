// Flags: --expose-internals
'use strict';

const common = require('../../common');
const assert = require('node:assert/strict');
const { internalBinding } = require('internal/test/binding');
const { createArrayBufferBatch } = internalBinding('buffer');
const { checkOwnership } = require(`./build/${common.buildType}/binding`);

const [source, sibling] = createArrayBufferBatch(1024, 2, 0);
const retained = new Uint8Array(sibling);
retained.fill(37);
const independent = checkOwnership(source, common.mustCall((alias) => {
  new Uint8Array(source)[0] = 19;
  assert.strictEqual(new Uint8Array(alias)[0], 19);
  source.transfer(0);
  alias.transfer(0);
}), () => {
  const [next] = createArrayBufferBatch(1024, 2, 0);
  new Uint8Array(next).fill(81);
  return next;
});
assert.strictEqual(independent, true);
assert(retained.every((byte) => byte === 37));
