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

[null, {}, [1], 'bad', true, 0.1].forEach((value) => {
  common.expectsError(() => {
    new vm.Script('void 0', { lineOffset: value });
  }, invalidArgType);

  common.expectsError(() => {
    new vm.Script('void 0', { columnOffset: value });
  }, invalidArgType);
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

  function assertErrors(options) {
    common.expectsError(() => {
      script.runInThisContext(options);
    }, invalidArgType);

    common.expectsError(() => {
      script.runInContext(sandbox, options);
    }, invalidArgType);

    common.expectsError(() => {
      script.runInNewContext({}, options);
    }, invalidArgType);
  }

  [null, 'bad', 42].forEach(assertErrors);
  [{}, [1], 'bad', null, -1, 0, NaN].forEach((value) => {
    assertErrors({ timeout: value });
  });
  [{}, [1], 'bad', 1, null].forEach((value) => {
    assertErrors({ displayErrors: value });
    assertErrors({ breakOnSigint: value });
  });
}
