import { expectWarning } from '../common/index.mjs';

expectWarning(
  'ExperimentalWarning',
  'Import assertions are not a stable feature of the JavaScript language. ' +
  'Avoid relying on their current behavior and syntax as those might change ' +
  'in a future version of Node.js.'
);

await import('data:text/javascript,', { assert: { someUnsupportedKey: 'value' } });
