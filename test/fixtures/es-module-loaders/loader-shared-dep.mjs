import dep from './loader-dep.js';
import assert from 'assert';

export function resolve(specifier, base, defaultResolve) {
  assert.equal(dep.format, 'esm');
  return defaultResolve(specifier, base);
}
