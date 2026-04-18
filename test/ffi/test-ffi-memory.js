// Flags: --experimental-ffi
'use strict';

const common = require('../common');
common.skipIfFFIMissing();
const { constants: bufferConstants } = require('node:buffer');
const assert = require('node:assert');
const { after, test } = require('node:test');
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require('./ffi-test-common');

const { lib, functions: symbols } = ffi.dlopen(libraryPath, {
  allocate_memory: fixtureSymbols.allocate_memory,
  deallocate_memory: fixtureSymbols.deallocate_memory,
});

after(() => lib.close());

function withAllocations(fn) {
  const allocations = new Set();

  function alloc(size) {
    const ptr = symbols.allocate_memory(BigInt(size));
    allocations.add(ptr);
    return ptr;
  }

  try {
    fn(alloc);
  } finally {
    for (const ptr of allocations) {
      symbols.deallocate_memory(ptr);
    }
  }
}

test('ffi reads and writes integer and float values', () => {
  withAllocations(common.mustCall((alloc) => {
    const ptr = alloc(64);

    ffi.setInt8(ptr, 0, -12);
    ffi.setUint8(ptr, 1, 250);
    ffi.setInt16(ptr, 2, -1234);
    ffi.setUint16(ptr, 4, 65000);
    ffi.setInt32(ptr, 8, -123456);
    ffi.setUint32(ptr, 12, 3000000000);
    ffi.setInt64(ptr, 16, -1234567890123n);
    ffi.setUint64(ptr, 24, 1234567890123n);
    ffi.setFloat32(ptr, 32, 3.5);
    ffi.setFloat64(ptr, 40, 7.25);

    assert.strictEqual(ffi.getInt8(ptr, 0), -12);
    assert.strictEqual(ffi.getUint8(ptr, 1), 250);
    assert.strictEqual(ffi.getInt16(ptr, 2), -1234);
    assert.strictEqual(ffi.getUint16(ptr, 4), 65000);
    assert.strictEqual(ffi.getInt32(ptr, 8), -123456);
    assert.strictEqual(ffi.getUint32(ptr, 12), 3000000000);
    assert.strictEqual(ffi.getInt64(ptr, 16), -1234567890123n);
    assert.strictEqual(ffi.getUint64(ptr, 24), 1234567890123n);
    assert.strictEqual(ffi.getFloat32(ptr, 32), 3.5);
    assert.strictEqual(ffi.getFloat64(ptr, 40), 7.25);
  }));
});

test('ffi supports unaligned memory access', () => {
  withAllocations(common.mustCall((alloc) => {
    const ptr = alloc(24);

    ffi.setInt32(ptr, 1, 0x12345678);
    ffi.setUint16(ptr, 7, 0xBEEF);
    ffi.setFloat64(ptr, 9, 6.5);

    assert.strictEqual(ffi.getInt32(ptr, 1), 0x12345678);
    assert.strictEqual(ffi.getUint16(ptr, 7), 0xBEEF);
    assert.strictEqual(ffi.getFloat64(ptr, 9), 6.5);
  }));
});

test('ffi toBuffer supports copy and zero-copy views', () => {
  withAllocations(common.mustCall((alloc) => {
    const ptr = alloc(8);
    ffi.exportBuffer(Buffer.from([1, 2, 3, 4]), ptr, 4);

    const copy = ffi.toBuffer(ptr, 4);
    const view = ffi.toBuffer(ptr, 4, false);

    assert.deepStrictEqual([...copy], [1, 2, 3, 4]);
    assert.deepStrictEqual([...view], [1, 2, 3, 4]);

    view[1] = 9;
    assert.deepStrictEqual([...ffi.toBuffer(ptr, 4)], [1, 9, 3, 4]);
    assert.deepStrictEqual([...copy], [1, 2, 3, 4]);

    const copyFromUndefined = ffi.toBuffer(ptr, 4, undefined);
    copyFromUndefined[0] = 77;
    assert.deepStrictEqual([...copyFromUndefined], [77, 9, 3, 4]);
    assert.deepStrictEqual([...ffi.toBuffer(ptr, 4)], [1, 9, 3, 4]);
  }));
});

test('ffi toArrayBuffer supports copy and zero-copy views', () => {
  withAllocations(common.mustCall((alloc) => {
    const ptr = alloc(4);
    ffi.exportBuffer(Buffer.from([10, 20, 30, 40]), ptr, 4);

    const copied = new Uint8Array(ffi.toArrayBuffer(ptr, 4));
    const shared = new Uint8Array(ffi.toArrayBuffer(ptr, 4, false));

    assert.deepStrictEqual([...copied], [10, 20, 30, 40]);
    assert.deepStrictEqual([...shared], [10, 20, 30, 40]);

    shared[2] = 99;
    assert.deepStrictEqual([...ffi.toBuffer(ptr, 4)], [10, 20, 99, 40]);
    assert.deepStrictEqual([...copied], [10, 20, 30, 40]);

    const copiedFromUndefined = new Uint8Array(ffi.toArrayBuffer(ptr, 4, undefined));
    copiedFromUndefined[1] = 55;
    assert.deepStrictEqual([...copiedFromUndefined], [10, 55, 99, 40]);
    assert.deepStrictEqual([...ffi.toBuffer(ptr, 4)], [10, 20, 99, 40]);
  }));
});

test('ffi exportString and exportBuffer copy data into native memory', () => {
  withAllocations(common.mustCall((alloc) => {
    const stringPtr = alloc(16);
    ffi.exportString('hello ffi', stringPtr, 16);
    assert.strictEqual(ffi.toString(stringPtr), 'hello ffi');

    assert.throws(() => ffi.exportString('truncated value', stringPtr, 5), {
      code: 'ERR_OUT_OF_RANGE',
    });

    ffi.exportBuffer(Buffer.from([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f]), stringPtr, 6);
    ffi.exportString('A', stringPtr, 6, 'utf16le');
    assert.deepStrictEqual([...ffi.toBuffer(stringPtr, 6)], [0x41, 0x00, 0x00, 0x00, 0x7f, 0x7f]);

    ffi.exportBuffer(Buffer.from([0x7f, 0x7f, 0x7f, 0x7f, 0x7f]), stringPtr, 5);
    assert.throws(() => ffi.exportString('AB', stringPtr, 5, 'utf16le'), {
      code: 'ERR_OUT_OF_RANGE',
    });

    ffi.exportString('AB', stringPtr, 6, 'utf16le');
    assert.deepStrictEqual([...ffi.toBuffer(stringPtr, 6)], [0x41, 0x00, 0x42, 0x00, 0x00, 0x00]);

    const bufferPtr = alloc(7);
    ffi.exportBuffer(Buffer.from([1, 2, 3, 4, 5, 6, 7]), bufferPtr, 7);
    assert.deepStrictEqual([...ffi.toBuffer(bufferPtr, 7)], [1, 2, 3, 4, 5, 6, 7]);
    assert.throws(() => ffi.exportBuffer(Buffer.from([1, 2, 3, 4, 5, 6, 7]), bufferPtr, 6), {
      code: 'ERR_OUT_OF_RANGE',
    });
  }));
});

test('ffi toString returns null for a null pointer', () => {
  assert.strictEqual(ffi.toString(0n), null);
});

test('ffi validates memory access arguments', () => {
  withAllocations(common.mustCall((alloc) => {
    const ptr = alloc(8);
    const maxPointer = process.arch === 'ia32' || process.arch === 'arm' ?
      0xfffffffcn :
      0xfffffffffffffffcn;

    assert.throws(() => ffi.toBuffer('nope', 4), /The first argument must be a bigint/);
    assert.throws(() => ffi.toBuffer(-1n, 4), /The first argument must be a non-negative bigint/);
    assert.throws(() => ffi.toBuffer(ptr, -1), /The length must be a non-negative integer/);
    assert.throws(() => ffi.toBuffer(0n, 1), /Cannot create a buffer from a null pointer/);
    assert.throws(() => ffi.toArrayBuffer(ptr, 'bad'), /The length must be a number/);
    assert.throws(() => ffi.toArrayBuffer(-1n, 4), /The first argument must be a non-negative bigint/);
    assert.throws(() => ffi.toArrayBuffer(0n, 1), /Cannot create an ArrayBuffer from a null pointer/);
    assert.throws(() => ffi.getInt32(0n), /Cannot dereference a null pointer/);
    assert.throws(() => ffi.getInt32(-1n), /The pointer must be a non-negative bigint/);
    assert.throws(() => ffi.getInt8(maxPointer, 8), /pointer and offset exceed the platform address range/);
    assert.throws(() => ffi.getInt32(maxPointer, 2), /accessed range exceeds the platform address range/);
    assert.throws(() => ffi.setUint8(ptr), /Expected an offset argument/);
    assert.throws(() => ffi.setUint8(-1n, 0, 1), /The pointer must be a non-negative bigint/);
    assert.throws(() => ffi.setUint8(maxPointer, 8, 1), /pointer and offset exceed the platform address range/);
    assert.throws(() => ffi.setUint32(maxPointer, 2, 1), /accessed range exceeds the platform address range/);
    assert.throws(() => ffi.setUint8(ptr, 0), /Expected a value argument/);
    assert.throws(() => ffi.setInt8(ptr, 0, 1.5), /Value must be an int8/);
    assert.throws(() => ffi.setInt8(ptr, 0, Number.NaN), /Value must be an int8/);
    assert.throws(() => ffi.setUint8(ptr, 0, 300), /Value must be a uint8/);
    assert.throws(() => ffi.setUint8(ptr, 0, Number.NaN), /Value must be a uint8/);
    assert.throws(() => ffi.setInt16(ptr, 'bad', 1), /The offset must be a number/);
    assert.throws(() => ffi.setInt16(ptr, 0, 1.5), /Value must be an int16/);
    assert.throws(() => ffi.setUint16(ptr, 0, Number.NaN), /Value must be a uint16/);
    assert.throws(() => ffi.setInt64(ptr, 0, 1.5), /Value must be an int64/);
    assert.throws(() => ffi.setInt64(ptr, 0, Number.NaN), /Value must be an int64/);
    assert.throws(() => ffi.setInt64(ptr, 0, 2n ** 63n), /Value must be an int64/);
    assert.throws(() => ffi.setInt64(ptr, 0, Number.MAX_SAFE_INTEGER + 1), /Value must be an int64/);
    assert.throws(() => ffi.setUint64(ptr, 0, -1), /Value must be a uint64/);
    assert.throws(() => ffi.setUint64(ptr, 0, 1.5), /Value must be a uint64/);
    assert.throws(() => ffi.setUint64(ptr, 0, -1n), /Value must be a uint64/);
    assert.throws(() => ffi.setUint64(ptr, 0, 2n ** 64n), /Value must be a uint64/);
    assert.throws(() => ffi.setUint64(ptr, 0, Number.MAX_SAFE_INTEGER + 1), /Value must be a uint64/);
    assert.throws(() => ffi.exportString(1, ptr, 4), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => ffi.exportString('ok', ptr, -1), { code: 'ERR_OUT_OF_RANGE' });
    assert.throws(() => ffi.exportString('ok', ptr, 4, 1), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => ffi.exportString('ok', ptr, 2), { code: 'ERR_OUT_OF_RANGE' });
    assert.throws(() => ffi.exportBuffer('bad', ptr, 4), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => ffi.exportBuffer(Buffer.from([1]), ptr, -1), { code: 'ERR_OUT_OF_RANGE' });
    assert.throws(() => ffi.exportBuffer(Buffer.from([1, 2]), ptr, 1), { code: 'ERR_OUT_OF_RANGE' });
    assert.throws(() => ffi.toBuffer(maxPointer, 8), /pointer and length exceed the platform address range/);
    assert.throws(() => ffi.toArrayBuffer(maxPointer, 8), /pointer and length exceed the platform address range/);
    assert.throws(() => ffi.toBuffer(1n, bufferConstants.MAX_LENGTH + 1), { code: 'ERR_BUFFER_TOO_LARGE' });
    assert.throws(() => ffi.toArrayBuffer(1n, bufferConstants.MAX_LENGTH + 1), { code: 'ERR_BUFFER_TOO_LARGE' });

    if (process.arch === 'ia32' || process.arch === 'arm') {
      assert.throws(() => ffi.toBuffer(2n ** 32n, 0), /platform pointer range/);
    }
  }));
});
