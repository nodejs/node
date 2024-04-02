// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js
// META: script=../resources/load_wasm.js

'use strict';

// https://webassembly.github.io/spec/js-api/

idl_test(
  ['wasm-js-api'],
  [],
  async idl_array => {
    self.mod = await createWasmModule();
    self.instance = new WebAssembly.Instance(self.mod);

    idl_array.add_objects({
      Memory: [new WebAssembly.Memory({initial: 1024})],
      Module: [self.mod],
      Instance: [self.instance],
    });
  }
);
