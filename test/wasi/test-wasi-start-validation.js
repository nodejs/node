// Flags: --experimental-wasi-unstable-preview0
'use strict';

require('../common');
const assert = require('assert');
const { WASI } = require('wasi');

const fixtures = require('../common/fixtures');

{
  const wasi = new WASI();
  assert.throws(
    () => {
      wasi.start();
    },
    { code: 'ERR_INVALID_ARG_TYPE', message: /\bWebAssembly\.Instance\b/ }
  );
}

{
  const wasi = new WASI({});
  (async () => {
    const bufferSource = fixtures.readSync('simple.wasm');
    const wasm = await WebAssembly.compile(bufferSource);
    const instance = await WebAssembly.instantiate(wasm);

    assert.throws(
      () => { wasi.start(instance); },
      { code: 'ERR_INVALID_ARG_TYPE', message: /\bWebAssembly\.Memory\b/ }
    );
  })();
}
