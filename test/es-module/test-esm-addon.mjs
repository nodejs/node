// Flags: --experimental-modules
/* eslint-disable required-modules */

import assert from 'assert';
import binding from '../addons/hello-world/build/Release/binding.node';
assert.strictEqual(binding.hello(), 'world');
console.log('binding.hello() =', binding.hello());
