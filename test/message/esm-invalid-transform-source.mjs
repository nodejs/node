// Flags: --experimental-loader ./test/fixtures/es-module-loaders/loader-invalid-transform-source.mjs
// Fixes: https://github.com/nodejs/node/issues/33441
import '../common/index.mjs';
import 'fs';
