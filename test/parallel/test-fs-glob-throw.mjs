import { mustCall } from '../common/index.mjs';
import { glob } from 'node:fs';
import process from 'node:process';
import { strictEqual } from 'node:assert';

// One uncaught error is expected
process.on('uncaughtException', mustCall((err) => {
  strictEqual(err.message, 'blep');
}));

{
  // Test that if callback throws, it's not getting called again
  glob('a/b/c', mustCall(() => {
    throw new Error('blep');
  }));
}
