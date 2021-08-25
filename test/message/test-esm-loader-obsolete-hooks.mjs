// Flags: --no-warnings --throw-deprecation --experimental-loader ./test/fixtures/es-module-loaders/hooks-obsolete.mjs
/* eslint-disable node-core/require-common-first, node-core/required-modules */

await import('whatever');
