// Flags: --experimental-modules
import '../common';
import { expectWarning } from '../common/index.mjs';
import assert from 'assert';
import process from 'process';

assert.strictEqual(Object.prototype.toString.call(process), '[object process]');
process[Symbol.toStringTag] = 'custom process';
expectWarning('DeprecationWarning',
              'Setting \'process[Symbol.toStringTag]\' is deprecated',
              'DEP0126');
assert.strictEqual(Object.prototype.toString.call(process),
                   '[object custom process]');
