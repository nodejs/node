// Flags: --experimental-loader ./test/fixtures/es-module-loaders/not-found-assert-loader.mjs
/* eslint-disable node-core/require-common-first, node-core/required-modules */
import './not-found.mjs';
