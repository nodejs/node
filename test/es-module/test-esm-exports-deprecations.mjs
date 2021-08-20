import { mustCall } from '../common/index.mjs';
import assert from 'assert';

let curWarning = 0;
const expectedWarnings = [
  '"./sub/"',
  '"./fallbackdir/"',
  '"./subpath/"',
  'no_exports',
  'default_index',
];

process.addListener('warning', mustCall((warning) => {
  assert(warning.stack.includes(expectedWarnings[curWarning++]), warning.stack);
}, expectedWarnings.length));

await import('./test-esm-exports.mjs');
