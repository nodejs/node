import { expectWarning, mustCall } from '../common/index.mjs';

expectWarning(
  'DeprecationWarning',
  'The multipleResolves event has been deprecated.',
  'DEPXXXX',
);

process.on('multipleResolves', mustCall());

new Promise((resolve) => {
  resolve();
  resolve();
});
