'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

const code = '1+1';

[
  -1,
  1.1,
  NaN,
  undefined,
  {},
  [],
  null,
  function() {},
  Symbol(),
  true,
  Infinity,
].forEach((compileOptions) => {
  try {
    new vm.Script(code, { filename: 'test.js', compileOptions });
  } catch (e) {
    assert.ok(/ERR_INVALID_ARG_VALUE/i.test(e.code));
  }
  try {
    vm.compileFunction(code, [], { filename: 'test.js', compileOptions });
  } catch (e) {
    assert.ok(/ERR_INVALID_ARG_VALUE/i.test(e.code));
  }
});

Object.values(vm.constants.COMPILE_OPTIONS).forEach((compileOptions) => {
  new vm.Script(code, {
    filename: `${compileOptions}.script.js`,
    compileOptions,
  });
  vm.compileFunction(code, [], {
    filename: `${compileOptions}.compileFunction.js`,
    compileOptions,
  });
});
