/* eslint-disable node-core/required-modules */
import '../fixtures/es-modules/test-esm-ok.mjs';

// We just test that this module doesn't fail loading

const flag = '--loader=./test/fixtures/es-module-loaders/loader-with-dep.mjs';
if (!process.execArgv.includes(flag)) {
  (async () => {
    const { requireFlags } = await import('../common');
    // Include `--experimental-modules` explicitly for workers.
    requireFlags('--experimental-modules', flag);
  })();
}
