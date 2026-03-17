// Flags: --experimental-ffi
'use strict';

const common = require('../common');
common.skipIfFFIMissing();

const {
  deepStrictEqual,
  strictEqual,
  throws,
} = require('node:assert');
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require('./ffi-test-common');

const { functions: symbols } = ffi.dlopen(libraryPath, {
  allocate_memory: fixtureSymbols.allocate_memory,
  deallocate_memory: fixtureSymbols.deallocate_memory,
});

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

withAllocations((alloc) => {
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

  strictEqual(ffi.getInt8(ptr, 0), -12);
  strictEqual(ffi.getUint8(ptr, 1), 250);
  strictEqual(ffi.getInt16(ptr, 2), -1234);
  strictEqual(ffi.getUint16(ptr, 4), 65000);
  strictEqual(ffi.getInt32(ptr, 8), -123456);
  strictEqual(ffi.getUint32(ptr, 12), 3000000000);
  strictEqual(ffi.getInt64(ptr, 16), -1234567890123n);
  strictEqual(ffi.getUint64(ptr, 24), 1234567890123n);
  strictEqual(ffi.getFloat32(ptr, 32), 3.5);
  strictEqual(ffi.getFloat64(ptr, 40), 7.25);
});

withAllocations((alloc) => {
  const ptr = alloc(24);

  ffi.setInt32(ptr, 1, 0x12345678);
  ffi.setUint16(ptr, 7, 0xBEEF);
  ffi.setFloat64(ptr, 9, 6.5);

  strictEqual(ffi.getInt32(ptr, 1), 0x12345678);
  strictEqual(ffi.getUint16(ptr, 7), 0xBEEF);
  strictEqual(ffi.getFloat64(ptr, 9), 6.5);
});

withAllocations((alloc) => {
  const ptr = alloc(8);
  ffi.exportBuffer(Buffer.from([1, 2, 3, 4]), ptr, 4);

  const copy = ffi.toBuffer(ptr, 4);
  const view = ffi.toBuffer(ptr, 4, false);

  deepStrictEqual([...copy], [1, 2, 3, 4]);
  deepStrictEqual([...view], [1, 2, 3, 4]);

  view[1] = 9;
  deepStrictEqual([...ffi.toBuffer(ptr, 4)], [1, 9, 3, 4]);
  deepStrictEqual([...copy], [1, 2, 3, 4]);

  const copyFromUndefined = ffi.toBuffer(ptr, 4, undefined);
  copyFromUndefined[0] = 77;
  deepStrictEqual([...copyFromUndefined], [77, 9, 3, 4]);
  deepStrictEqual([...ffi.toBuffer(ptr, 4)], [1, 9, 3, 4]);
});

withAllocations((alloc) => {
  const ptr = alloc(4);
  ffi.exportBuffer(Buffer.from([10, 20, 30, 40]), ptr, 4);

  const copied = new Uint8Array(ffi.toArrayBuffer(ptr, 4));
  const shared = new Uint8Array(ffi.toArrayBuffer(ptr, 4, false));

  deepStrictEqual([...copied], [10, 20, 30, 40]);
  deepStrictEqual([...shared], [10, 20, 30, 40]);

  shared[2] = 99;
  deepStrictEqual([...ffi.toBuffer(ptr, 4)], [10, 20, 99, 40]);
  deepStrictEqual([...copied], [10, 20, 30, 40]);

  const copiedFromUndefined = new Uint8Array(ffi.toArrayBuffer(ptr, 4, undefined));
  copiedFromUndefined[1] = 55;
  deepStrictEqual([...copiedFromUndefined], [10, 55, 99, 40]);
  deepStrictEqual([...ffi.toBuffer(ptr, 4)], [10, 20, 99, 40]);
});

withAllocations((alloc) => {
  const stringPtr = alloc(16);
  ffi.exportString('hello ffi', stringPtr, 16);
  strictEqual(ffi.toString(stringPtr), 'hello ffi');

  ffi.exportString('truncated value', stringPtr, 5);
  strictEqual(ffi.toString(stringPtr), 'trun');

  const bufferPtr = alloc(6);
  ffi.exportBuffer(Buffer.from([1, 2, 3, 4, 5, 6, 7]), bufferPtr, 6);
  deepStrictEqual([...ffi.toBuffer(bufferPtr, 6)], [1, 2, 3, 4, 5, 6]);
});

{
  strictEqual(ffi.toString(0n), null);
}

withAllocations((alloc) => {
  const ptr = alloc(8);
  const maxPointer = process.arch === 'ia32' || process.arch === 'arm' ?
    0xfffffffcn :
    0xfffffffffffffffcn;

  throws(() => ffi.toBuffer('nope', 4), /The first argument must be a bigint/);
  throws(() => ffi.toBuffer(-1n, 4), /The first argument must be a non-negative bigint/);
  throws(() => ffi.toBuffer(ptr, -1), /The length must be a non-negative integer/);
  throws(() => ffi.toBuffer(0n, 1), /Cannot create a buffer from a null pointer/);
  throws(() => ffi.toArrayBuffer(ptr, 'bad'), /The length must be a number/);
  throws(() => ffi.toArrayBuffer(-1n, 4), /The first argument must be a non-negative bigint/);
  throws(() => ffi.toArrayBuffer(0n, 1), /Cannot create an ArrayBuffer from a null pointer/);
  throws(() => ffi.getInt32(0n), /Cannot dereference a null pointer/);
  throws(() => ffi.getInt32(-1n), /The pointer must be a non-negative bigint/);
  throws(() => ffi.getInt8(maxPointer, 8), /pointer and offset exceed the platform address range/);
  throws(() => ffi.setUint8(ptr), /Expected an offset argument/);
  throws(() => ffi.setUint8(-1n, 0, 1), /The pointer must be a non-negative bigint/);
  throws(() => ffi.setUint8(maxPointer, 8, 1), /pointer and offset exceed the platform address range/);
  throws(() => ffi.setUint8(ptr, 0), /Expected a value argument/);
  throws(() => ffi.setInt8(ptr, 0, 1.5), /Value must be an int8/);
  throws(() => ffi.setInt8(ptr, 0, Number.NaN), /Value must be an int8/);
  throws(() => ffi.setUint8(ptr, 0, 300), /Value must be a uint8/);
  throws(() => ffi.setUint8(ptr, 0, Number.NaN), /Value must be a uint8/);
  throws(() => ffi.setInt16(ptr, 'bad', 1), /The offset must be a number/);
  throws(() => ffi.setInt16(ptr, 0, 1.5), /Value must be an int16/);
  throws(() => ffi.setUint16(ptr, 0, Number.NaN), /Value must be a uint16/);
  throws(() => ffi.setInt64(ptr, 0, 1.5), /Value must be an int64/);
  throws(() => ffi.setInt64(ptr, 0, Number.NaN), /Value must be an int64/);
  throws(() => ffi.setInt64(ptr, 0, 2n ** 63n), /Value must be an int64/);
  throws(() => ffi.setInt64(ptr, 0, Number.MAX_SAFE_INTEGER + 1), /Value must be an int64/);
  throws(() => ffi.setUint64(ptr, 0, -1), /Value must be a uint64/);
  throws(() => ffi.setUint64(ptr, 0, 1.5), /Value must be a uint64/);
  throws(() => ffi.setUint64(ptr, 0, -1n), /Value must be a uint64/);
  throws(() => ffi.setUint64(ptr, 0, 2n ** 64n), /Value must be a uint64/);
  throws(() => ffi.setUint64(ptr, 0, Number.MAX_SAFE_INTEGER + 1), /Value must be a uint64/);
  throws(() => ffi.exportString(1, ptr, 4), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  throws(() => ffi.exportString('ok', ptr, -1), {
    code: 'ERR_OUT_OF_RANGE',
  });
  throws(() => ffi.exportString('ok', ptr, 4, 1), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  throws(() => ffi.exportBuffer('bad', ptr, 4), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  throws(() => ffi.exportBuffer(Buffer.from([1]), ptr, -1), {
    code: 'ERR_OUT_OF_RANGE',
  });

  if (process.arch === 'ia32' || process.arch === 'arm') {
    throws(() => ffi.toBuffer(2n ** 32n, 0), /platform pointer range/);
  }
});
