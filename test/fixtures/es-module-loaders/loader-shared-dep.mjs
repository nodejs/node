import assert from 'assert';

import {createRequire} from '../../common';

const require = createRequire(import.meta.url);
const dep = require('./loader-dep.js');

export function resolve(specifier, base, defaultResolve) {
  assert.strictEqual(dep.format, 'esm');
  return defaultResolve(specifier, base);
}
