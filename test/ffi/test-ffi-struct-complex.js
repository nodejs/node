// Flags: --experimental-ffi
'use strict';
const common = require('../common');
const { describe, it } = require('node:test');
const assert = require('node:assert');
const path = require('node:path');

common.skipIfFFIMissing();

const { dlopen } = require('node:ffi');

const libraryPath = path.join(
  __dirname,
  'fixture_library',
  'build',
  'Release',
  process.platform === 'win32' ? 'ffi_test_library.dll' :
    process.platform === 'darwin' ? 'ffi_test_library.dylib' :
      'ffi_test_library.so',
);

describe('FFI nested and float struct descriptors', () => {
  it('supports float struct roundtrip through result bytes', () => {
    const lib = dlopen(libraryPath, {
      make_point3d: {
        parameters: ['f32', 'f32', 'f32'],
        result: { struct: ['f32', 'f32', 'f32'] },
      },
      point3d_magnitude_squared: {
        parameters: [{ struct: ['f32', 'f32', 'f32'] }],
        result: 'f32',
      },
    });

    const p = lib.symbols.make_point3d(1.5, 2.0, 2.5);
    assert.ok(p instanceof Uint8Array);
    assert.strictEqual(p.byteLength, 12);

    const result = lib.symbols.point3d_magnitude_squared(p);
    assert.ok(Math.abs(result - 12.5) < 0.0001);
  });
});
