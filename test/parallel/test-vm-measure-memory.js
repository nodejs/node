'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');
const {
  SUMMARY,
  DETAILED
} = vm.constants.measureMemory.mode;

common.expectWarning('ExperimentalWarning',
                     'vm.measureMemory is an experimental feature. ' +
                     'This feature could change at any time');

// Test measuring memory of the current context
{
  vm.measureMemory(undefined)
    .then((result) => {
      assert(result instanceof Object);
    });

  vm.measureMemory({})
    .then((result) => {
      assert(result instanceof Object);
    });

  vm.measureMemory({ mode: SUMMARY })
    .then((result) => {
      assert(result instanceof Object);
    });

  vm.measureMemory({ mode: DETAILED })
    .then((result) => {
      assert(result instanceof Object);
    });

  assert.throws(() => vm.measureMemory(null), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => vm.measureMemory('summary'), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => vm.measureMemory({ mode: -1 }), {
    code: 'ERR_OUT_OF_RANGE'
  });
}

// Test measuring memory of the sandbox
{
  const sandbox = vm.createContext();
  vm.measureMemory(undefined, sandbox)
    .then((result) => {
      assert(result instanceof Object);
    });

  vm.measureMemory({}, sandbox)
    .then((result) => {
      assert(result instanceof Object);
    });

  vm.measureMemory({ mode: SUMMARY }, sandbox)
    .then((result) => {
      assert(result instanceof Object);
    });

  vm.measureMemory({ mode: DETAILED }, sandbox)
    .then((result) => {
      assert(result instanceof Object);
    });

  assert.throws(() => vm.measureMemory({ mode: SUMMARY }, null), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
}
