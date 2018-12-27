/* eslint-disable node-core/required-modules */
import assert from 'assert';
import ok from '../fixtures/es-modules/test-esm-ok.mjs';

const flag = '--loader=./test/fixtures/es-module-loaders/js-loader.mjs';
if (!process.execArgv.includes(flag)) {
  (async () => {
    const { requireFlags } = await import('../common');
    // Include `--experimental-modules` explicitly for workers.
    requireFlags(['--experimental-modules', flag]);
  })();
} else {
  const test = async () => {
    const { namedExport } =
      await import('../fixtures/es-module-loaders/js-as-esm.js');
    assert(ok);
    assert(namedExport);
  };
  test().catch((e) => { console.log(e); process.exit(1); });
}
