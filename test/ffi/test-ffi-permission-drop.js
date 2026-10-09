// Flags: --permission --allow-ffi --allow-fs-read=*
'use strict';

const common = require('../common');
const { fixtureSymbols, libraryPath } = require('./ffi-test-common');

common.skipIfFFIMissing();

const ffi = require('node:ffi');
const assert = require('assert');

function openLibrary() {
  const { lib } = ffi.dlopen(libraryPath, {
    add_i32: fixtureSymbols.add_i32,
    allocate_memory: fixtureSymbols.allocate_memory,
    deallocate_memory: fixtureSymbols.deallocate_memory,
  });
  lib.close();
}


{
  assert.ok(process.permission.has('ffi'));
}

{
  // shouldNotThrow
  openLibrary();
}

{
  const { lib } = ffi.dlopen(libraryPath);
  const address = lib.getSymbol('add_i32');
  const signature = { arguments: ['i32', 'i32'], return: 'i32' };
  assert.strictEqual(lib.toFunction(address, signature)(20, 22), 42);
  assert.throws(() => lib.toFunction(address, {
    get arguments() {
      process.permission.drop('ffi');
      return ['i32', 'i32'];
    },
    return: 'i32',
  }), common.expectsError({ code: 'ERR_ACCESS_DENIED', permission: 'FFI' }));
  assert.throws(() => lib.toFunction(address, signature),
                common.expectsError({ code: 'ERR_ACCESS_DENIED', permission: 'FFI' }));
  lib.close();
  process.permission.drop('ffi');
  assert.ok(!process.permission.has('ffi'));
  assert.throws(() => {
    openLibrary();
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
  }));
}
