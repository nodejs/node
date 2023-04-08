import '../common/index.mjs';
import { describe, it } from 'node:test';
import * as assert from 'node:assert';
import * as path from 'node:path';

describe('path.glob', () => {
  it('should return a filter function', () => {
    const fn = path.glob('*.js');
    assert.strictEqual(typeof fn, 'function');
  });
  it('should return a filter function that matches the glob', () => {
    const fn = path.glob('*.js');
    assert.strictEqual(fn('foo.js'), true);
  });
});

describe('path.globToRegExp', () => {
  it('should return a RegExp', () => {
    const re = path.globToRegExp('foo');
    assert.strictEqual(re instanceof RegExp, true);
  });
  it('should return a RegExp that matches the glob', () => {
    const re = path.globToRegExp('*.js');
    assert.strictEqual(re.test('foo.js'), true);
  });
});
