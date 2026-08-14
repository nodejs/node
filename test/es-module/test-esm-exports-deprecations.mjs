// Flags: --pending-deprecation
import { mustCall } from '../common/index.mjs';
import assert from 'assert';

let curWarning = 0;
// DEP0151 index lookup and extension searching now throw instead of warning.
// Slash-pattern deprecations are no longer emitted by test-esm-exports.mjs.
const expectedWarnings = [];

process.addListener('warning', mustCall((warning) => {
  assert(warning.stack.includes(expectedWarnings[curWarning++]), warning.stack);
}, expectedWarnings.length));

await import('./test-esm-exports.mjs');
