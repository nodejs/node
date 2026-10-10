// Flags: --jitless
'use strict';

const common = require('../common');
common.skipIfFFIMissing();
if (!common.isLinux) {
  common.skip('This test requires Linux seccomp');
}
if (require('node:os').endianness() !== 'LE') {
  common.skip('The seccomp fixture reads little-endian syscall arguments');
}
if (!['x64', 'arm64', 'ppc64', 'riscv64', 'loong64', 's390x'].includes(process.arch)) {
  common.skip('No Fast API stub emitter for this architecture');
}
const assert = require('node:assert');
const ffi = require('node:ffi');
const { libraryPath, fixtureSymbols } = require('./ffi-test-common');

const lib = new ffi.DynamicLibrary(libraryPath);
try {
  const deny = lib.getFunction('deny_executable_memory', {
    arguments: ['function'],
    return: 'i32',
  });
  // SAFETY: The fixture ignores the null function pointer and only restricts
  // executable mappings in this jitless test thread.
  const result = deny(null);
  if (result !== 0) {
    common.skip(`Cannot install seccomp filter: ${result}`);
  }

  // Repeated rejection must not cache a callable or apply a different signature.
  for (let i = 0; i < 2; i++) {
    assert.throws(() => lib.getFunction('add_i32', fixtureSymbols.add_i32), {
      code: 'ERR_RX_MEMORY_NOT_SUPPORTED',
      message: 'Executable memory is not supported in this environment, but is required for FFI Fast API calls',
    });
  }
  assert.throws(() => lib.getFunction('identity_pointer', {
    arguments: ['pointer'], return: 'pointer',
  }), { code: 'ERR_RX_MEMORY_NOT_SUPPORTED' });

  const fallback = lib.getFunction('identity_i64_fallback', {
    arguments: ['pointer', 'i64', 'function'],
    return: 'i64',
  });
  // SAFETY: The fixture ignores both null pointers and returns the scalar value.
  assert.strictEqual(fallback(null, 42n, null), 42n);

  // Static libffi trampolines may still work. Dynamic closure failures must
  // report the missing capability instead of an opaque allocation error.
  let callback;
  try {
    callback = lib.registerCallback(() => {});
  } catch (err) {
    assert.strictEqual(err.code, 'ERR_RX_MEMORY_NOT_SUPPORTED');
    assert.strictEqual(err.message,
                       'Executable memory is not supported in this environment, but is required for FFI callbacks');
  }
  if (callback !== undefined) {
    lib.unregisterCallback(callback);
  }
} finally {
  lib.close();
}
