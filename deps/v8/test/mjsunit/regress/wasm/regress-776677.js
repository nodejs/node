// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function module(stdlib,foreign,buffer) {
  "use asm";
  var fl = new stdlib.Uint32Array(buffer);
  function f1(x) {
    x = x | 0;
    fl[0] = x;
    fl[0x10000] = x;
    fl[0x100000] = x;
  }
  return f1;
}

var global = {Uint32Array:Uint32Array};
var env = {};
memory = new WebAssembly.Memory({initial:200});
var buffer = memory.buffer;
evil_f = module(global,env,buffer);

zz = {};
zz.toString = function() {
  Array.prototype.slice.call([]);
  return 0xffffffff;
}
evil_f(3);
assertThrows(() => memory.grow(1), RangeError);
evil_f(zz);
