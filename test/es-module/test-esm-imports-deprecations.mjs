// Flags: --pending-deprecation
import { mustCall } from '../common/index.mjs';
import assert from 'assert';

let curWarning = 0;
const expectedWarnings = [
  'Use of deprecated double slash',
  'Use of deprecated double slash',
  './sub//null',
  './sub/////null',
  './sub//internal/test',
  './sub//internal//test',
  '#subpath/////internal',
  '#subpath//asdf.asdf',
  '#subpath/as//df.asdf',
  './sub//null',
  './sub/////null',
  './sub//internal/test',
  './sub//internal//test',
  '#subpath/////internal',
];

process.addListener('warning', mustCall((warning) => {
  assert(warning.stack.includes(expectedWarnings[curWarning++]), warning.stack);
}, expectedWarnings.length));

await import('./test-esm-imports.mjs');
