// Flags: --experimental-modules
import '../common';
import assert from 'assert';
import process from 'process';

assert.strictEqual(Object.prototype.toString.call(process), '[object process]');
