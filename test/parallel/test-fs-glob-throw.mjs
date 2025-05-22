import { mustCall } from '../common/index.mjs';
import { glob } from 'node:fs';
import process from 'node:process';
import { strictEqual } from 'node:assert';

// One uncaught error is expected
process.on('uncaughtException', mustCall((err) => {
  strictEqual(err.message, 'blep');
}));

{
  let callCount = 0;
  const callback = mustCall(() => {
    if (callCount++ === 0) {
      throw new Error('blep');
    }
  });

  // Test that if callback throws, it's not getting called again
  glob('a/b/c', callback);
}
