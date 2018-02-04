// Flags: --experimental-modules --loader ./test/fixtures/es-module-loaders/not-found-assert-loader.mjs
/* eslint-disable node-core/required-modules */
import './not-found.js';
