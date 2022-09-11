// Flags: --pending-deprecation
import { mustCall } from '../common/index.mjs';
import assert from 'assert';

let curWarning = 0;
const expectedWarnings = [
  'Use of deprecated leading or trailing slash',
  'Use of deprecated double slash',
  './/asdf.js',
  '".//internal/test.js"',
  '".//internal//test.js"',
  '"./////internal/////test.js"',
  '"./trailing-pattern-slash/"',
  '"./subpath/dir1/dir1.js"',
  '"./subpath//dir1/dir1.js"',
  './/asdf.js',
  '".//internal/test.js"',
  '".//internal//test.js"',
  '"./////internal/////test.js"',
  'no_exports',
  'default_index',
];

process.addListener('warning', mustCall((warning) => {
  assert(warning.stack.includes(expectedWarnings[curWarning++]), warning.stack);
}, expectedWarnings.length));

await import('./test-esm-exports.mjs');
