import { spawnPromisified } from '../common/index.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';

await Promise.all([
  // Setting importAssertions on the context object of the load hook should warn but still work.
  `data:text/javascript,export ${encodeURIComponent(function load(u, c, n) {
    if (u !== 'data:') return n(u, c);
    c.importAssertions.type = 'json';
    return n('data:application/json,1', c);
  })}`,
  // Creating a new context object with importAssertions in the load hook should warn but still work.
  `data:text/javascript,export ${encodeURIComponent(function load(u, c, n) {
    if (u !== 'data:') return n(u, c);
    c.importAssertions = { type: 'json' };
    return n('data:application/json,1', c);
  })}`,
].map(async (loaderURL) => {
  const { stdout, stderr, code } = await spawnPromisified(execPath, [
    '--input-type=module',
    '--experimental-loader',
    loaderURL,
    '--eval', `
    import assert from 'node:assert';
    
    assert.deepStrictEqual(
      { ...await import('data:') },
      { default: 1 }
      );`,
  ]);

  assert.match(stderr, /Use `importAttributes` instead of `importAssertions`/);
  assert.strictEqual(stdout, '');
  assert.strictEqual(code, 0);
}));
