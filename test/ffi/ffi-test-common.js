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
  add_i8: { parameters: ['i8', 'i8'], result: 'i8' },
  add_u8: { parameters: ['u8', 'u8'], result: 'u8' },
  add_i16: { parameters: ['i16', 'i16'], result: 'i16' },
  add_u16: { parameters: ['u16', 'u16'], result: 'u16' },
  add_i32: { parameters: ['i32', 'i32'], result: 'i32' },
  add_u32: { parameters: ['u32', 'u32'], result: 'u32' },
  add_i64: { parameters: ['i64', 'i64'], result: 'i64' },
  add_u64: { parameters: ['u64', 'u64'], result: 'u64' },
  identity_char: { parameters: ['char'], result: 'char' },
  char_is_signed: { parameters: [], result: 'i32' },
  add_f32: { parameters: ['f32', 'f32'], result: 'f32' },
  multiply_f64: { parameters: ['f64', 'f64'], result: 'f64' },
  identity_pointer: { parameters: ['pointer'], result: 'pointer' },
  pointer_to_usize: { parameters: ['pointer'], result: 'u64' },
  usize_to_pointer: { parameters: ['u64'], result: 'pointer' },
  string_length: { parameters: ['pointer'], result: 'u64' },
  string_concat: { parameters: ['pointer', 'pointer'], result: 'pointer' },
  string_duplicate: { parameters: ['pointer'], result: 'pointer' },
  free_string: { parameters: ['pointer'], result: 'void' },
  fill_buffer: { parameters: ['pointer', 'u64', 'u32'], result: 'void' },
  sum_buffer: { parameters: ['pointer', 'u64'], result: 'u64' },
  reverse_buffer: { parameters: ['pointer', 'u64'], result: 'void' },
  logical_and: { parameters: ['i32', 'i32'], result: 'i32' },
  logical_or: { parameters: ['i32', 'i32'], result: 'i32' },
  logical_not: { parameters: ['i32'], result: 'i32' },
  increment_counter: { parameters: [], result: 'void' },
  get_counter: { parameters: [], result: 'i32' },
  reset_counter: { parameters: [], result: 'void' },
  call_int_callback: { parameters: ['pointer', 'i32'], result: 'i32' },
  call_int8_callback: { parameters: ['pointer', 'i8'], result: 'i8' },
  call_pointer_callback_is_null: { parameters: ['pointer'], result: 'i32' },
  call_void_callback: { parameters: ['pointer'], result: 'void' },
  call_string_callback: { parameters: ['function', 'pointer'], result: 'void' },
  call_binary_int_callback: { parameters: ['function', 'i32', 'i32'], result: 'i32' },
  call_callback_multiple_times: { parameters: ['pointer', 'i32'], result: 'void' },
  divide_i32: { parameters: ['i32', 'i32'], result: 'i32' },
  safe_strlen: { parameters: ['pointer'], result: 'i32' },
  sum_five_i32: { parameters: ['i32', 'i32', 'i32', 'i32', 'i32'], result: 'i32' },
  sum_five_f64: { parameters: ['f64', 'f64', 'f64', 'f64', 'f64'], result: 'f64' },
  mixed_operation: { parameters: ['i32', 'f32', 'f64', 'u32'], result: 'f64' },
  allocate_memory: { parameters: ['u64'], result: 'pointer' },
  deallocate_memory: { parameters: ['pointer'], result: 'void' },
  array_get_i32: { parameters: ['pointer', 'u64'], result: 'i32' },
  array_set_i32: { parameters: ['pointer', 'u64', 'i32'], result: 'void' },
  array_get_f64: { parameters: ['pointer', 'u64'], result: 'f64' },
  array_set_f64: { parameters: ['pointer', 'u64', 'f64'], result: 'void' },
};

if (!common.isWindows) {
  fixtureSymbols.readonly_memory = { parameters: [], result: 'pointer' };
}

function cString(value) {
  return Buffer.from(`${value}\0`);
}

module.exports = {
  cString,
  fixtureSymbols,
  libraryPath,
};
