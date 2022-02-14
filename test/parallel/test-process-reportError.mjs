import { mustCall, mustNotCall } from '../common/index.mjs';
import { strictEqual, match } from 'assert';
import { once } from 'events';

{
  // Verify uncaughtException is called
  process.on('unhandledRejection', mustNotCall());
  const e = new Error('boom');
  queueMicrotask(() => reportError(e));
  const [ error ] = await once(process, 'uncaughtException');
  strictEqual(error, e);
}

{
  // Verify warning is called when emitting non-error
  process.once('warning', mustCall((warning) => {
    match(warning.stack, /namedFunctionForStackTrace/);
    strictEqual(warning.name, 'Warning');
    strictEqual(warning.message, 'reportError was called with a non-error.');
  }));
  queueMicrotask(async function namedFunctionForStackTrace() {
    reportError(null);
  });
  const [ error ] = await once(process, 'uncaughtException');
  strictEqual(error, null);
}
