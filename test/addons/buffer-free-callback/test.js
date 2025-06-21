'use strict';
// Flags: --expose-gc

const common = require('../../common');
const binding = require(`./build/${common.buildType}/binding`);

function check(size, alignment, offset) {
  let buf = binding.alloc(size, alignment, offset);
  let slice = buf.slice(size >>> 1);

  buf = null;
  binding.check(slice);
  slice = null;
  global.gc();
  global.gc();
  global.gc();
}

// NOTE: If adding more check() test cases,
// be sure to not duplicate alignment/offset.
// Refs: https://github.com/nodejs/node/issues/31061#issuecomment-568612283

check(64, 1, 0);

// Buffers can have weird sizes.
check(97, 1, 1);

// Buffers can be unaligned
check(64, 8, 0);
check(64, 16, 0);
check(64, 8, 1);
check(64, 16, 1);
check(97, 8, 3);
check(97, 16, 3);
check(97, 8, 5);
check(97, 16, 5);

// Empty ArrayBuffer does not allocate data, worth checking
check(0, 1, 2);
