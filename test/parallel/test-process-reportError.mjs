import { mustCall, mustNotCall } from '../common/index.mjs';
import { strictEqual } from 'assert';
{
  // Verify uncaughtException is called
  process.on('unhandledRejection', mustNotCall());
  const e = new Error('boom');
  process.on('uncaughtException', mustCall((error) => {
    strictEqual(error, e);
  }));
  reportError(e);
}
