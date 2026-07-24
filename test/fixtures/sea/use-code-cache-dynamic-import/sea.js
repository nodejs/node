(async () => {
  const assert = require('node:assert');

  // Dynamic import of a built-in module should work even with code cache.
  const { strictEqual } = await import('node:assert');
  assert.strictEqual(strictEqual, assert.strictEqual);

  // Dynamic import of another built-in module.
  const { join } = await import('node:path');
  assert.strictEqual(typeof join, 'function');

  console.log('dynamic import with code cache works');
})();
