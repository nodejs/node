'use strict';

const common = require('../common');
const vm = require('vm');

const invalidArgType = {
  type: TypeError,
  code: 'ERR_INVALID_ARG_TYPE'
};

const outOfRange = {
  type: RangeError,
  code: 'ERR_OUT_OF_RANGE'
};

common.expectsError(() => {
  new vm.Script('void 0', 42);
}, invalidArgType);

[null, {}, [1], 'bad', true].forEach((value) => {
  common.expectsError(() => {
    new vm.Script('void 0', { lineOffset: value });
  }, invalidArgType);

  common.expectsError(() => {
    new vm.Script('void 0', { columnOffset: value });
  }, invalidArgType);
});

[0.1, 2 ** 32].forEach((value) => {
  common.expectsError(() => {
    new vm.Script('void 0', { lineOffset: value });
  }, outOfRange);

  common.expectsError(() => {
    new vm.Script('void 0', { columnOffset: value });
  }, outOfRange);
});

common.expectsError(() => {
  new vm.Script('void 0', { lineOffset: Number.MAX_SAFE_INTEGER });
}, outOfRange);

common.expectsError(() => {
  new vm.Script('void 0', { columnOffset: Number.MAX_SAFE_INTEGER });
}, outOfRange);

common.expectsError(() => {
  new vm.Script('void 0', { filename: 123 });
}, invalidArgType);

common.expectsError(() => {
  new vm.Script('void 0', { produceCachedData: 1 });
}, invalidArgType);

[[0], {}, true, 'bad', 42].forEach((value) => {
  common.expectsError(() => {
    new vm.Script('void 0', { cachedData: value });
  }, invalidArgType);
});

{
  const script = new vm.Script('void 0');
  const sandbox = vm.createContext();

  function assertErrors(options, errCheck) {
    common.expectsError(() => {
      script.runInThisContext(options);
    }, errCheck);

    common.expectsError(() => {
      script.runInContext(sandbox, options);
    }, errCheck);

    common.expectsError(() => {
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
