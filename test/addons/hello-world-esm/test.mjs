/* eslint-disable required-modules */

import assert from 'assert';
import binding from './build/binding.node';
assert.strictEqual(binding.hello(), 'world');
console.log('binding.hello() =', binding.hello());
