'use strict';

require('../common');
const assert = require('assert');

const someBindingPath = './test/addons/hello-world/build/Release/binding.node';

assert.throws(() => {
  process.dlopen({ exports: undefined }, someBindingPath);
}, Error);
