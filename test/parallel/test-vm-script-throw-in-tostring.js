'use strict';

require('../common');
const assert = require('assert');

const vm = require('vm');

assert.throws(() => {
  new vm.Script({
    toString() {
      throw new Error();
    }
  });
}, Error);
