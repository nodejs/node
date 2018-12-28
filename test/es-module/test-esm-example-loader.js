/* eslint-disable node-core/required-modules */
if (typeof require === 'function') {
  const common = require('../common');
  common.requireFlags(
    '--experimental-modules',
    '--loader=./test/fixtures/es-module-loaders/example-loader.mjs'
  );
} else {
  async function test() {
    const { default: assert } = await import('assert');
    const ok = await import('../fixtures/es-modules/test-esm-ok.mjs');
    assert(ok);
  }

  test().catch((e) => { console.error(e); process.exit(1); });
}
