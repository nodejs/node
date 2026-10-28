'use strict';

// This tests that module.registerHooks() hooks are invoked for require(esm)
// and for the CommonJS and ES module graphs reachable from it.

const common = require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');
const assert = require('assert');

function assertSubGraph(output) {
  assert.match(output, /resolve \.\/esm\.mjs from file:.+main\.cjs/);
  assert.match(output, /load file:.+esm\.mjs/);

  assert.match(output, /resolve \.\/inner\.mjs from file:.+esm\.mjs/);
  assert.match(output, /load file:.+inner\.mjs/);

  assert.match(output, /resolve \.\/cjs\.cjs from file:.+esm\.mjs/);
  assert.match(output, /load file:.+cjs\.cjs/);

  assert.match(output, /resolve \.\/inner\.cjs from file:.+cjs\.cjs/);
  assert.match(output, /load file:.+inner\.cjs/);

  assert.match(output, /esmValue in main\.cjs: esm/);
  assert.match(output, /cjsValue in main\.cjs: commonjs/);
}

spawnSyncAndAssert(process.execPath, [
  '--import',
  fixtures.fileURL('module-hooks', 'register-logger-hooks.mjs'),
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
  fixtures.fileURL('module-hooks', 'register-logger-hooks.mjs'),
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

    assert.match(output, /resolve \.\/main\.cjs from file:.+main\.mjs/);
    assert.match(output, /load file:.+main\.cjs/);

    assertSubGraph(output);
  }),
});
