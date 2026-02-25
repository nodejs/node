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

describe('FFI structs via descriptors', () => {
  it('returns a struct as Uint8Array for result: { struct: [...] }', () => {
    const lib = dlopen(libraryPath, {
      make_point: {
        parameters: ['i32', 'i32'],
        result: { struct: ['i32', 'i32'] },
      },
    });

    const bytes = lib.symbols.make_point(12, 34);
    assert.ok(bytes instanceof Uint8Array);
    assert.strictEqual(bytes.byteLength, 8);

    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    assert.strictEqual(view.getInt32(0, true), 12);
    assert.strictEqual(view.getInt32(4, true), 34);
  });

  it('accepts struct parameter as TypedArray for parameters: [{ struct: [...] }]', () => {
    const lib = dlopen(libraryPath, {
      point_distance_squared: {
        parameters: [{ struct: ['i32', 'i32'] }, { struct: ['i32', 'i32'] }],
        result: 'i32',
      },
    });

    const p1 = new Uint8Array(8);
    const p1View = new DataView(p1.buffer);
    p1View.setInt32(0, 3, true);
    p1View.setInt32(4, 4, true);

    const p2 = new Uint8Array(8);
    const p2View = new DataView(p2.buffer);
    p2View.setInt32(0, 0, true);
    p2View.setInt32(4, 0, true);

    assert.strictEqual(lib.symbols.point_distance_squared(p1, p2), 25);
  });
});
