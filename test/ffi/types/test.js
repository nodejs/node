'use strict';
const common = require('../../common');
const assert = require('assert');
const path = require('path');
const os = require('os');
const ffi = require('node:ffi');

const ext = common.isOSX ? 'dylib' : common.isWindows ? 'dll' : 'so';

const shared = path.join(__dirname, `./build/${common.buildType}/types.${ext}`);

const basicTypes = [
  'char',
  'signed char',
  'unsigned char',
  'short',
  'short int',
  'signed short',
  'signed short int',
  'unsigned short',
  'unsigned short int',
  'int',
  'signed',
  'signed int',
  'unsigned',
  'unsigned int',
  'long',
  'long int',
  'signed long',
  'signed long int',
  'unsigned long',
  'unsigned long int',
  'long long',
  'long long int',
  'signed long long',
  'signed long long int',
  'unsigned long long',
  'unsigned long long int',
  'float',
  'double',
  'uint8_t',
  'int8_t',
  'uint16_t',
  'int16_t',
  'uint32_t',
  'int32_t',
  'uint64_t',
  'int64_t',
];

function getRWName(type, size) {
  const end = size > 1 ? os.endianness() : '';
  if (type === 'float') return `Float${end}`;
  if (type === 'double') return `Double${end}`;
  const big = size >= 8 ? 'Big' : '';
  const u = type.startsWith('u') && size < 8 ? 'U' : '';
  return `${u}${big}Int${size * 8}${end}`;
}

function isBigInt(type) {
  return type !== 'double' && ffi.sizeof(type) === 8;
}

for (const type of basicTypes) {
  const fnName = `test_add_${type.replace(/ /g, '_')}`;
  const fn = ffi.getNativeFunction(shared, fnName, type, [type, type]);
  const three = isBigInt(type) ? 3n : 3;
  const four = isBigInt(type) ? 4n : 4;
  const seven = isBigInt(type) ? 7n : 7;
  assert.strictEqual(fn(three, four), seven);

  const ptrFnName = `test_add_ptr_${type.replace(/ /g, '_')}`;
  const ptrType = type + '*';
  assert.strictEqual(ffi.sizeof(ptrType), ffi.sizeof('void *'));
  const ptrFn = ffi.getNativeFunction(shared, ptrFnName, ptrType, [ptrType, ptrType, ptrType]);
  const size = ffi.sizeof(type);
  const buf = Buffer.alloc(size * 3);
  const retPtr = ffi.getBufferPointer(buf);
  const rwName = getRWName(type, size);
  buf[`write${rwName}`](three, size);
  buf[`write${rwName}`](four, 2 * size);
  const ret = ptrFn(retPtr, retPtr + BigInt(size), retPtr + BigInt(2 * size));
  assert.strictEqual(buf[`read${rwName}`](0), seven);
  assert.strictEqual(ret, retPtr);
}
