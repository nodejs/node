// Flags: --experimental-modules --loader ./test/fixtures/es-module-loaders/loader-with-dep.mjs
/* eslint-disable required-modules */
import './test-esm-ok.mjs';

// We just test that this module doesn't fail loading
