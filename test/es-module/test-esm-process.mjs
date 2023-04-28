import '../common/index.mjs';
import assert from 'assert';
import process, { report } from 'process';

assert.strictEqual(Object.prototype.toString.call(process), '[object process]');
assert(Object.getOwnPropertyDescriptor(process, Symbol.toStringTag).writable);
assert(report);
