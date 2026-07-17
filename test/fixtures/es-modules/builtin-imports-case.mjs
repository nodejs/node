import { strictEqual } from 'assert';
import './dep1.js';
import { assert as depAssert } from './dep2.js';
strictEqual(depAssert.strictEqual, strictEqual);
console.log('ok');
