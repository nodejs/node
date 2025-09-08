'use strict';

// This tests that the async loader hooks can be invoked for require(esm).

require('../common');
const common = require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');
const assert = require('assert');

function assertSubGraph(output) {
  // FIXME(node:59666): the async resolve hook invoked for require() in imported CJS only get the URL,
  // not the specifier. This may be fixable if we re-implement the async loader hooks from within
  // the synchronous loader hooks and use the original require() implementation.
  assert.match(output, /resolve .+esm\.mjs from file:.+main\.cjs/);
  assert.match(output, /load file:.+esm\.mjs/);

  assert.match(output, /resolve \.\/inner\.mjs from file:.+esm\.mjs/);
  assert.match(output, /load file:.+inner\.mjs/);

  assert.match(output, /resolve \.\/cjs\.cjs from file:.+esm\.mjs/);
  assert.match(output, /load file:.+cjs\.cjs/);

  // FIXME(node:59666): see above.
  assert.match(output, /resolve .+inner\.cjs from file:.+cjs\.cjs/);
  assert.match(output, /load file:.+inner\.cjs/);

  assert.match(output, /esmValue in main\.cjs: esm/);
  assert.match(output, /cjsValue in main\.cjs: commonjs/);
}

spawnSyncAndAssert(process.execPath, [
  '--import',
  fixtures.fileURL('module-hooks', 'register-logger-async-hooks.mjs'),
  fixtures.path('module-hooks', 'require-esm', 'main.cjs'),
], {
  stdout: common.mustCall((output) => {
    // The graph is:
    //   main.cjs
    //     -> esm.mjs
    //       -> inner.mjs
    //       -> cjs.cjs
    //         -> inner.cjs
    assert.match(output, /resolve .+main\.cjs from undefined/);
    assert.match(output, /load file:.+main\.cjs/);

    assertSubGraph(output);
  }),
});

spawnSyncAndAssert(process.execPath, [
  '--import',
  fixtures.fileURL('module-hooks', 'register-logger-async-hooks.mjs'),
  fixtures.path('module-hooks', 'require-esm', 'main.mjs'),
], {
  stdout: common.mustCall((output) => {
    // The graph is:
    //   main.mjs
    //     -> main.cjs
    //       -> esm.mjs
    //         -> inner.mjs
    //         -> cjs.cjs
    //           -> inner.cjs
    assert.match(output, /resolve .+main\.mjs from undefined/);
    assert.match(output, /load file:.+main\.mjs/);

    assert.match(output, /resolve .+main\.cjs from file:.+main\.mjs/);
    assert.match(output, /load file:.+main\.cjs/);

    assertSubGraph(output);
  }),
});
