/* eslint-disable node-core/required-modules */
const flag =
  '--loader=./test/fixtures/es-module-loaders/not-found-assert-loader.mjs';

if (!process.execArgv.includes(flag)) {
  (async () => {
    const { requireFlags } = await import('../common');
    // Include `--experimental-modules` explicitly for workers.
    requireFlags(['--experimental-modules', flag]);
  })();
} else {
  import('./not-found').catch((e) => { console.error(e); process.exit(1); });
}
