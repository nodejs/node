// js file that is common.js
import a from './a.js';
// ESM with named export
import {b} from './b.mjs';
// import 'c.cjs';
import cjs from './c.cjs';
// proves cross boundary fun bits
import jsAsEsm from '../package-type-module/a.js';

// named export from core
import {strictEqual, deepStrictEqual} from 'assert';

strictEqual(a, jsAsEsm);
strictEqual(b, 'b');
deepStrictEqual(cjs, {
  one: 1,
  two: 2,
  three: 3
});

export default 'commonjs';
