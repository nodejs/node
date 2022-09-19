// ESM with only default
import a from './a.js';
// ESM with named export
import {b} from './b.mjs';
// import 'c.cjs';
import cjs from './c.cjs';
// import across boundaries
import jsAsCjs from '../package-type-commonjs/a.js'

// named export from core
import {strictEqual, deepStrictEqual} from 'assert';

strictEqual(a, jsAsCjs);
strictEqual(b, 'b');
deepStrictEqual(cjs, {
  one: 1,
  two: 2,
  three: 3
});

export default 'module';
