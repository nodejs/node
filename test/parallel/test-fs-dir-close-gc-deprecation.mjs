// Flags: --expose-gc --no-warnings

// Test that a runtime warning is emitted when a Dir object
// is allowed to close on garbage collection. In the future, this
// test should verify that closing on garbage collection throws a
// process fatal exception.

import { expectWarning, mustCall } from '../common/index.mjs';
import { rejects } from 'node:assert';
import { opendir } from 'node:fs/promises';
import { setImmediate } from 'node:timers';

const warning =
  'Closing a Dir object on garbage collection is deprecated. ' +
  'Please close Dir objects explicitly using Dir.prototype.close(), ' +
  "or by using explicit resource management ('using' keyword). " +
  'In the future, an error will be thrown if a directory is closed during garbage collection.';

// Test that closing Dir automatically using iterator doesn't emit warning
{
  const dir = await opendir(import.meta.dirname);
  await Array.fromAsync(dir);
  await rejects(dir.close(), { code: 'ERR_DIR_CLOSED' });
}

{
  expectWarning({
    Warning: [['Closing directory handle on garbage collection']],
    DeprecationWarning: [[warning, 'DEP0200']],
  });

  await opendir(import.meta.dirname).then(mustCall(() => {
    setImmediate(() => {
      // The dir is out of scope now
      globalThis.gc();

      // Wait an extra event loop turn, as the warning is emitted from the
      // native layer in an unref()'ed setImmediate() callback.
      setImmediate(mustCall());
    });
  }));
}
