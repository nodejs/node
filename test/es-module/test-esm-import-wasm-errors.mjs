// Flags: --experimental-modules

import { crashOnUnhandledRejection } from '../common';
import assert from 'assert';

crashOnUnhandledRejection();

async function expectsRejection(fn, settings) {
  // Retain async context.
  const storedError = new Error('Thrown from:');
  try {
    await fn();
  } catch (err) {
    try {
      assert(err instanceof settings.type,
             `${err.name} is not instance of ${settings.type.name}`);
      assert.strictEqual(err.name, settings.type.name);
    } catch (validationError) {
      console.error(validationError);
      console.error('Original error:');
      console.error(err);
      throw storedError;
    }
    return;
  }
  assert.fail('Missing expected exception');
}

expectsRejection(() =>
  import('../fixtures/es-modules/invalid.wasm'), {
  type: WebAssembly.CompileError,
});
