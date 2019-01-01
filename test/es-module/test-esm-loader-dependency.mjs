/* eslint-disable node-core/required-modules */
import '../fixtures/es-modules/test-esm-ok.mjs';

// We just test that this module doesn't fail loading

(async () => {
  const { requireFlags } = await import('../common');
  requireFlags(
    '--experimental-modules',
    '--loader=./test/fixtures/es-module-loaders/loader-with-dep.mjs'
  );
})();
