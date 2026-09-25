'use strict';

require('../common');
const assert = require('assert');
const vm = require('vm');

const invalidArgType = {
  name: 'TypeError',
  code: 'ERR_INVALID_ARG_TYPE'
};

const outOfRange = {
  name: 'RangeError',
  code: 'ERR_OUT_OF_RANGE'
};

assert.throws(() => {
  new vm.Script('void 0', 42);
}, invalidArgType);

[null, {}, [1], 'bad', true].forEach((value) => {
  assert.throws(() => {
    new vm.Script('void 0', { lineOffset: value });
  }, invalidArgType);

  assert.throws(() => {
    new vm.Script('void 0', { columnOffset: value });
  }, invalidArgType);
});

[0.1, 2 ** 32].forEach((value) => {
  assert.throws(() => {
    new vm.Script('void 0', { lineOffset: value });
  }, outOfRange);

  assert.throws(() => {
    new vm.Script('void 0', { columnOffset: value });
  }, outOfRange);
});

assert.throws(() => {
  new vm.Script('void 0', { lineOffset: Number.MAX_SAFE_INTEGER });
}, outOfRange);

assert.throws(() => {
  new vm.Script('void 0', { columnOffset: Number.MAX_SAFE_INTEGER });
}, outOfRange);

assert.throws(() => {
  new vm.Script('void 0', { filename: 123 });
}, invalidArgType);

assert.throws(() => {
  new vm.Script('void 0', { produceCachedData: 1 });
}, invalidArgType);

[[0], {}, true, 'bad', 42].forEach((value) => {
  assert.throws(() => {
    new vm.Script('void 0', { cachedData: value });
  }, invalidArgType);
});

{
  const script = new vm.Script('void 0');
  const sandbox = vm.createContext();

  function assertErrors(options, errCheck) {
    assert.throws(() => {
      script.runInThisContext(options);
    }, errCheck);

    assert.throws(() => {
      script.runInContext(sandbox, options);
    }, errCheck);

    assert.throws(() => {
      script.runInNewContext({}, options);
    }, errCheck);
  }

  [null, 'bad', 42].forEach((value) => {
    assertErrors(value, invalidArgType);
  });
  [{}, [1], 'bad', null].forEach((value) => {
    assertErrors({ timeout: value }, invalidArgType);
  });
  [-1, 0, NaN].forEach((value) => {
    assertErrors({ timeout: value }, outOfRange);
  });
  [{}, [1], 'bad', 1, null].forEach((value) => {
    assertErrors({ displayErrors: value }, invalidArgType);
    assertErrors({ breakOnSigint: value }, invalidArgType);
  });
}
