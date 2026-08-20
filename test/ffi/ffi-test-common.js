'use strict';

const common = require('../common');
const fs = require('node:fs');
const path = require('node:path');

common.skipIfFFIMissing();

const { suffix } = require('node:ffi');

const fixtureBuildDir = path.join(
  __dirname,
  'fixture_library',
  'build',
  common.buildType,
);
const libraryPath = path.join(fixtureBuildDir, `ffi_test_library.${suffix}`);

function ensureFixtureLibrary() {
  if (!fs.existsSync(libraryPath)) {
    throw new Error(
      `Missing FFI test fixture library: ${libraryPath}. ` +
      'Build it first with `make build-ffi-tests` or the equivalent test build step.',
    );
  }
}

ensureFixtureLibrary();

const fixtureSymbols = {
  add_i8: { arguments: ['int8', 'int8'], return: 'int8' },
  add_u8: { arguments: ['uint8', 'uint8'], return: 'uint8' },
  add_i16: { arguments: ['int16', 'int16'], return: 'int16' },
  add_u16: { arguments: ['uint16', 'uint16'], return: 'uint16' },
  add_i32: { arguments: ['int32', 'int32'], return: 'int32' },
  add_u32: { arguments: ['uint32', 'uint32'], return: 'uint32' },
  add_i64: { arguments: ['int64', 'int64'], return: 'int64' },
  add_u64: { arguments: ['uint64', 'uint64'], return: 'uint64' },
  identity_char: { arguments: ['char'], return: 'char' },
  char_is_signed: { arguments: [], return: 'int32' },
  add_f32: { arguments: ['float32', 'float32'], return: 'float32' },
  multiply_f64: { arguments: ['float64', 'float64'], return: 'float64' },
  identity_pointer: { arguments: ['pointer'], return: 'pointer' },
  pointer_to_usize: { arguments: ['pointer'], return: 'uint64' },
  usize_to_pointer: { arguments: ['uint64'], return: 'pointer' },
  string_length: { arguments: ['pointer'], return: 'uint64' },
  string_concat: { arguments: ['pointer', 'pointer'], return: 'pointer' },
  string_duplicate: { arguments: ['pointer'], return: 'pointer' },
  free_string: { arguments: ['pointer'], return: 'void' },
  fill_buffer: { arguments: ['pointer', 'uint64', 'uint32'], return: 'void' },
  sum_buffer: { arguments: ['pointer', 'uint64'], return: 'uint64' },
  reverse_buffer: { arguments: ['pointer', 'uint64'], return: 'void' },
  logical_and: { arguments: ['int32', 'int32'], return: 'int32' },
  logical_or: { arguments: ['int32', 'int32'], return: 'int32' },
  logical_not: { arguments: ['int32'], return: 'int32' },
  increment_counter: { arguments: [], return: 'void' },
  get_counter: { arguments: [], return: 'int32' },
  reset_counter: { arguments: [], return: 'void' },
  call_int_callback: { arguments: ['pointer', 'int32'], return: 'int32' },
  call_int8_callback: { arguments: ['pointer', 'int8'], return: 'int8' },
  call_pointer_callback_is_null: { arguments: ['pointer'], return: 'int32' },
  call_void_callback: { arguments: ['pointer'], return: 'void' },
  call_string_callback: { arguments: ['function', 'pointer'], return: 'void' },
  call_binary_int_callback: { arguments: ['function', 'int32', 'int32'], return: 'int32' },
  call_callback_multiple_times: { arguments: ['pointer', 'int32'], return: 'void' },
  divide_i32: { arguments: ['int32', 'int32'], return: 'int32' },
  safe_strlen: { arguments: ['pointer'], return: 'int32' },
  sum_five_i32: { arguments: ['int32', 'int32', 'int32', 'int32', 'int32'], return: 'int32' },
  sum_five_f64: { arguments: ['float64', 'float64', 'float64', 'float64', 'float64'], return: 'float64' },
  mixed_operation: { arguments: ['int32', 'float32', 'float64', 'uint32'], return: 'float64' },
  allocate_memory: { arguments: ['uint64'], return: 'pointer' },
  deallocate_memory: { arguments: ['pointer'], return: 'void' },
  array_get_i32: { arguments: ['pointer', 'uint64'], return: 'int32' },
  array_set_i32: { arguments: ['pointer', 'uint64', 'int32'], return: 'void' },
  array_get_f64: { arguments: ['pointer', 'uint64'], return: 'float64' },
  array_set_f64: { arguments: ['pointer', 'uint64', 'float64'], return: 'void' },
};

if (!common.isWindows) {
  fixtureSymbols.readonly_memory = { arguments: [], return: 'pointer' };
}

function cString(value) {
  return Buffer.from(`${value}\0`);
}

module.exports = {
  cString,
  fixtureSymbols,
  libraryPath,
};
