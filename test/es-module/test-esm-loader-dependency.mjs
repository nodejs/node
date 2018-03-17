// Flags: --experimental-modules --loader ./test/fixtures/es-module-loaders/loader-with-dep.mjs
/* eslint-disable node-core/required-modules */
import '../fixtures/es-modules/test-esm-ok.mjs';

// We just test that this module doesn't fail loading
