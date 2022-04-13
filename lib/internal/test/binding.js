'use strict';

const {
  globalThis,
} = primordials;

process.emitWarning(
  'These APIs are for internal testing only. Do not use them.',
  'internal/test/binding');

if (module.isPreloading) {
  globalThis.internalBinding = internalBinding;
  globalThis.primordials = primordials;
}

module.exports = { internalBinding, primordials };
