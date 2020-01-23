// Flags: --experimental-loader ./test/fixtures/es-module-loaders/syntax-error.mjs
import '../common/index.mjs';
console.log('This should not be printed');
