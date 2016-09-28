'use strict';
// Flags: --expose-gc

const common = require('../../common');
const binding = require(`./build/${common.buildType}/binding`);

function check(size) {
  var buf = binding.alloc(size);
  var slice = buf.slice(size >>> 1);

  buf = null;
  binding.check(slice);
  slice = null;
  gc();
  gc();
  gc();
}

check(64);

// Empty ArrayBuffer does not allocate data, worth checking
check(0);
