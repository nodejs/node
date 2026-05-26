'use strict';

const common = require('../common');
const fs = require('node:fs');
const path = require('node:path');

common.skipIfFFIMissing();

const fixtureBuildDir = path.join(
  __dirname,
  'fixture_library',
  'build',
  common.buildType,
);
const libraryPath = path.join(
  fixtureBuildDir,
  process.platform === 'win32' ? 'ffi_test_library.dll' :
    process.platform === 'darwin' ? 'ffi_test_library.dylib' :
      'ffi_test_library.so',
);

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
  add_i8: { arguments: ['i8', 'i8'], return: 'i8' },
  add_u8: { arguments: ['u8', 'u8'], return: 'u8' },
  add_i16: { arguments: ['i16', 'i16'], return: 'i16' },
  add_u16: { arguments: ['u16', 'u16'], return: 'u16' },
  add_i32: { arguments: ['i32', 'i32'], return: 'i32' },
  add_u32: { arguments: ['u32', 'u32'], return: 'u32' },
  add_i64: { arguments: ['i64', 'i64'], return: 'i64' },
  add_u64: { arguments: ['u64', 'u64'], return: 'u64' },
  identity_char: { arguments: ['char'], return: 'char' },
  char_is_signed: { arguments: [], return: 'i32' },
  add_f32: { arguments: ['f32', 'f32'], return: 'f32' },
  multiply_f64: { arguments: ['f64', 'f64'], return: 'f64' },
  identity_pointer: { arguments: ['pointer'], return: 'pointer' },
  pointer_to_usize: { arguments: ['pointer'], return: 'u64' },
  usize_to_pointer: { arguments: ['u64'], return: 'pointer' },
  string_length: { arguments: ['pointer'], return: 'u64' },
  string_concat: { arguments: ['pointer', 'pointer'], return: 'pointer' },
  string_duplicate: { arguments: ['pointer'], return: 'pointer' },
  free_string: { arguments: ['pointer'], return: 'void' },
  fill_buffer: { arguments: ['pointer', 'u64', 'u32'], return: 'void' },
  sum_buffer: { arguments: ['pointer', 'u64'], return: 'u64' },
  reverse_buffer: { arguments: ['pointer', 'u64'], return: 'void' },
  logical_and: { arguments: ['i32', 'i32'], return: 'i32' },
  logical_or: { arguments: ['i32', 'i32'], return: 'i32' },
  logical_not: { arguments: ['i32'], return: 'i32' },
  increment_counter: { arguments: [], return: 'void' },
  get_counter: { arguments: [], return: 'i32' },
  reset_counter: { arguments: [], return: 'void' },
  call_int_callback: { arguments: ['pointer', 'i32'], return: 'i32' },
  call_int8_callback: { arguments: ['pointer', 'i8'], return: 'i8' },
  call_pointer_callback_is_null: { arguments: ['pointer'], return: 'i32' },
  call_void_callback: { arguments: ['pointer'], return: 'void' },
  call_string_callback: { arguments: ['function', 'pointer'], return: 'void' },
  call_binary_int_callback: { arguments: ['function', 'i32', 'i32'], return: 'i32' },
  call_callback_multiple_times: { arguments: ['pointer', 'i32'], return: 'void' },
  divide_i32: { arguments: ['i32', 'i32'], return: 'i32' },
  safe_strlen: { arguments: ['pointer'], return: 'i32' },
  sum_five_i32: { arguments: ['i32', 'i32', 'i32', 'i32', 'i32'], return: 'i32' },
  sum_five_f64: { arguments: ['f64', 'f64', 'f64', 'f64', 'f64'], return: 'f64' },
  mixed_operation: { arguments: ['i32', 'f32', 'f64', 'u32'], return: 'f64' },
  allocate_memory: { arguments: ['u64'], return: 'pointer' },
  deallocate_memory: { arguments: ['pointer'], return: 'void' },
  array_get_i32: { arguments: ['pointer', 'u64'], return: 'i32' },
  array_set_i32: { arguments: ['pointer', 'u64', 'i32'], return: 'void' },
  array_get_f64: { arguments: ['pointer', 'u64'], return: 'f64' },
  array_set_f64: { arguments: ['pointer', 'u64', 'f64'], return: 'void' },
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
