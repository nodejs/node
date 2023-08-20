// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

PAGE_SIZE = 0x10000;
PAGES = 10;

memory = new WebAssembly.Memory({initial: PAGES});
buffer = memory.buffer;

var func = (function (stdlib, env, heap) {
    "use asm";

    var array = new stdlib.Int32Array(heap);

    return function () {
        array[0] = 0x41424344;
        array[1] = 0x45464748;
    }
}({Int32Array: Int32Array}, {}, buffer));

for (var i = 0; i < 1000; ++i)
    func();

memory.grow(1);

func();

for(var i = 0; i < 2; ++i)
    new ArrayBuffer(PAGE_SIZE * PAGES);
