import assert from 'assert';

import {createRequire} from '../../common/index.mjs';

const require = createRequire(import.meta.url);
const dep = require('./loader-dep.js');

export function resolve() {
  assert.strictEqual(dep.format, 'module');
  return null;
}
